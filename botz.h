#ifndef _BOTZ_H_
#define _BOTZ_H_
#include <stddef.h>
#include "evx.h"
#include "hash.h"

struct botz_listen {
  struct evx_listen bl_listen;
  struct list_head bl_conn_list;
  /* TODO bl_max_conn */
  double bl_timeout;
  size_t bl_rd_buf_size, bl_wr_hdr_size, bl_wr_buf_size;
  struct hash_table bl_res_table;
};

int botz_listen_init(struct botz_listen *bl, size_t nr_res);

void botz_listen_destroy(struct botz_listen *bl);

struct json;

int botz_set_json(struct botz_listen *bl, const char *path,
                  struct json *j, void *data);

int botz_del_res(struct botz_listen *bl, const char *path);

#endif
