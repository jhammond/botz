#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include "botz.h"
#include "hash.h"
#include "list.h"
#include "n_buf.h"
#include "string1.h"
#include "json.h"
#include "trace.h"

#define BOTZ_OK 200
#define BOTZ_BAD_REQUEST 400
#define BOTZ_NOT_FOUND 404
#define BOTZ_METHOD_NOT_ALLOWED 405
#define BOTZ_REQUEST_TIMEOUT 408
#define BOTZ_INTERVAL_SERVER_ERROR 500
#define BOTZ_NOT_IMPLEMENTED 501

static const char *botz_strstatus(int status)
{
  switch (status) {
  case BOTZ_OK:
    return "OK";
  case BOTZ_BAD_REQUEST:
    return "Bad Request";
  case BOTZ_NOT_FOUND:
    return "Not Found";
  case BOTZ_METHOD_NOT_ALLOWED:
    return "Method Not Allowed";
  case BOTZ_REQUEST_TIMEOUT:
    return "Request Timeout";
  case BOTZ_INTERVAL_SERVER_ERROR:
    return "Interval Server Error";
  case BOTZ_NOT_IMPLEMENTED:
    return "Not Implemented";
  default:
    return "Botzed";
  }
}

struct botz_res {
  struct hlist_node br_hash_node;
  struct json *br_json;
  void *br_data;
  char br_path[];
};

struct botz_conn {
  struct ev_io bc_io_w;
  struct ev_timer bc_timer_w;
  struct n_buf bc_rd_buf, bc_wr_hdr, bc_wr_buf;
  struct botz_listen *bc_listen;
  struct list_head bc_listen_link;
};

static void botz_conn_cb(EV_P_ struct evx_listen *el, int fd,
                         const struct sockaddr *addr, socklen_t addrlen);
static void botz_conn_rd_cb(EV_P_ struct ev_io *w, int revents);
static void botz_conn_timer_cb(EV_P_ struct ev_timer *w, int revents);
static void botz_conn_kill(EV_P_ struct botz_conn *bc);

int botz_listen_init(struct botz_listen *bl, size_t nr_res)
{
  memset(bl, 0, sizeof(*bl));
  evx_listen_init(&bl->bl_listen, &botz_conn_cb);
  INIT_LIST_HEAD(&bl->bl_conn_list);
  bl->bl_timeout = 15.0; /* XXX */
  bl->bl_rd_buf_size = 4096;
  bl->bl_wr_hdr_size = 4096;
  bl->bl_wr_buf_size = 1048576; /* XXX */

  if (hash_table_init(&bl->bl_res_table, nr_res) < 0)
    return -1;

  return 0;
}

void botz_listen_destroy(struct botz_listen *bl)
{
  /* ... */
  memset(bl, 0, sizeof(*bl));
}

int botz_set_json(struct botz_listen *bl, const char *path,
                  struct json *j, void *data)
{
  struct hash_table *t = &bl->bl_res_table;
  struct hlist_head *head;
  struct botz_res *br;

  br = str_table_lookup_entry(t, path, &head, struct botz_res,
                              br_hash_node, br_path);
  if (br != NULL) {
    errno = EEXIST;
    return -1;
  }

  br = malloc(sizeof(*br) + strlen(path) + 1);
  if (br == NULL)
    return -1;

  memset(br, 0, sizeof(*br));
  strcpy(br->br_path, path);
  br->br_json = j;
  br->br_data = data;
  hlist_add_head(&br->br_hash_node, head);

  return 0;
}

static struct botz_res *
botz_lookup_res(struct botz_listen *bl, const char *path, int *status)
{
  struct hash_table *t = &bl->bl_res_table;
  struct hlist_head *head;
  struct botz_res *br;

  TRACE("path `%s'\n", path);

  br = str_table_lookup_entry(t, path, &head, struct botz_res,
                              br_hash_node, br_path);
  if (br == NULL)
    *status = BOTZ_NOT_FOUND;

  return br;
}

static void botz_conn_kill(EV_P_ struct botz_conn *bc)
{
  TRACE("botz_conn_kill fd %d\n", bc->bc_io_w.fd);

  ev_io_stop(EV_A_ &bc->bc_io_w);
  ev_timer_stop(EV_A_ &bc->bc_timer_w);

  if (!(bc->bc_io_w.fd < 0))
    close(bc->bc_io_w.fd);
  bc->bc_io_w.fd = -1;

  list_del(&bc->bc_listen_link);
  n_buf_destroy(&bc->bc_rd_buf);
  n_buf_destroy(&bc->bc_wr_hdr);
  n_buf_destroy(&bc->bc_wr_buf);

  free(bc);
}

static void botz_conn_cb(EV_P_ struct evx_listen *el, int fd,
                         const struct sockaddr *addr, socklen_t addrlen)
{
  struct botz_listen *bl = container_of(el, struct botz_listen, bl_listen);
  struct botz_conn *bc = NULL;

  bc = malloc(sizeof(*bc));
  if (bc == NULL)
    goto err;

  memset(bc, 0, sizeof(*bc));

  ev_io_init(&bc->bc_io_w, &botz_conn_rd_cb, fd, EV_READ);
  ev_io_start(EV_A_ &bc->bc_io_w);

  ev_timer_init(&bc->bc_timer_w, &botz_conn_timer_cb, bl->bl_timeout, 0);
  ev_timer_start(EV_A_ &bc->bc_timer_w);

  bc->bc_listen = bl;
  list_add(&bc->bc_listen_link, &bl->bl_conn_list);

  if (n_buf_init(&bc->bc_rd_buf, bl->bl_rd_buf_size) < 0)
    goto err;
  if (n_buf_init(&bc->bc_wr_hdr, bl->bl_wr_hdr_size) < 0)
    goto err;
  if (n_buf_init(&bc->bc_wr_buf, bl->bl_wr_buf_size) < 0)
    goto err;

  return;

 err:
  if (bc != NULL)
    botz_conn_kill(EV_A_ bc);
}

static void botz_conn_fmt_hdr(struct botz_conn *bc, int status)
{
  if (status < 0)
    status = BOTZ_INTERVAL_SERVER_ERROR;
  else if (status == 0)
    status = BOTZ_OK;

  n_buf_clear(&bc->bc_wr_hdr);
  n_buf_printf(&bc->bc_wr_hdr,
               "HTTP/1.1 %d %s\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: %zu\r\n"
               "Connection: close\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "\r\n", status, botz_strstatus(status),
               n_buf_length(&bc->bc_wr_buf));
}

static void botz_conn_error(EV_P_ struct botz_conn *bc, int status)
{
  int eof = 0, err = 0;

  n_buf_clear(&bc->bc_wr_buf);
  botz_conn_fmt_hdr(bc, status);
  n_buf_drain(&bc->bc_wr_hdr, bc->bc_io_w.fd, &eof, &err);
  botz_conn_kill(EV_A_ bc);
}

static void botz_conn_timer_cb(EV_P_ struct ev_timer *w, int revents)
{
  struct botz_conn *bc = container_of(w, struct botz_conn, bc_timer_w);

  botz_conn_error(EV_A_ bc, BOTZ_REQUEST_TIMEOUT);
}

static void botz_conn_wr_cb(EV_P_ struct ev_io *w, int revents)
{
  struct botz_conn *bc = container_of(w, struct botz_conn, bc_io_w);
  int eof = 0, err = 0;

  n_buf_drain(&bc->bc_wr_hdr, w->fd, &eof, &err);
  if (!eof)
    return;
  if (err != 0) {
    botz_conn_error(EV_A_ bc, -1);
    return;
  }

  eof = 0;
  n_buf_drain(&bc->bc_wr_buf, w->fd, &eof, &err);
  if (!eof)
    return;
  if (err != 0) {
    botz_conn_error(EV_A_ bc, -1);
    return;
  }

  botz_conn_kill(EV_A_ bc);
}

static void botz_conn_rd_cb(EV_P_ struct ev_io *w, int revents)
{
  struct botz_conn *bc = container_of(w, struct botz_conn, bc_io_w);
  int eof = 0, err = 0;
  char *req, *meth, *path;
  size_t req_len;
  struct botz_res *br;

  n_buf_fill(&bc->bc_rd_buf, w->fd, &eof, &err);
  if (err != 0) {
    botz_conn_error(EV_A_ bc, -1);
    return;
  }

  if (n_buf_get_msg(&bc->bc_rd_buf, &req, &req_len) != 0) {
    if (eof)
      botz_conn_error(EV_A_ bc, BOTZ_BAD_REQUEST);
    return;
  }

  if (split(&req, &meth, &path, (char *) NULL) != 2) {
    botz_conn_error(EV_A_ bc, BOTZ_BAD_REQUEST);
    return;
  }

  if (strcmp(meth, "GET") != 0) {
    botz_conn_error(EV_A_ bc, BOTZ_NOT_IMPLEMENTED);
    return;
  }

  int status = 0;
  br = botz_lookup_res(bc->bc_listen, path, &status);
  if (br == NULL) {
    botz_conn_error(EV_A_ bc, status);
    return;
  }

  ssize_t len = 0;
  len += json_format(&bc->bc_wr_buf, br->br_json, br->br_data);
  len += n_buf_printf(&bc->bc_wr_buf, "\r\n"); /* How many CRLF's? */

  if (len > bc->bc_wr_buf.nb_size) {
    botz_conn_error(EV_A_ bc, -1); /* XXX status. */
    return;
  }

  botz_conn_fmt_hdr(bc, BOTZ_OK);

  ev_io_stop(EV_A_ &bc->bc_io_w);
  ev_io_init(&bc->bc_io_w, &botz_conn_wr_cb, w->fd, EV_WRITE);
  ev_io_start(EV_A_ &bc->bc_io_w);
}
