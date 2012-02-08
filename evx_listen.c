#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include "evx.h"
#include "trace.h"

#ifndef container_of
#define container_of(ptr, type, member) ({                       \
      const typeof(((type *) 0)->member) *__mptr = (ptr);        \
      (type *) ((char *) __mptr - offsetof(type, member));       \
    })
#endif

static inline void fd_set_nonblock(int fd)
{
  long flags = fcntl(fd, F_GETFL);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static inline void fd_set_cloexec(int fd)
{
  long flags = fcntl(fd, F_GETFD);
  fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static void evx_listen_io_cb(EV_P_ struct ev_io *w, int revents)
{
  struct evx_listen *el = container_of(w, struct evx_listen, el_io_w);
  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof(addr);
  int fd = -1;

  if (revents & EV_ERROR)
    /* TODO.  Note el_io_w is stopped. */;

  /* TODO.  Use accept4() if available. */
  fd = accept(w->fd, (struct sockaddr *) &addr, &addrlen);
  if (fd < 0) {
    if (!(errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED))
      ERROR("cannot accept connections: %s\n", strerror(errno));
    /* ... */
    return;
  }

  if (el->el_nonblock)
    fd_set_nonblock(fd);

  if (el->el_cloexec)
    fd_set_cloexec(fd);

  (*el->el_connect_cb)(EV_A_ el, fd, (struct sockaddr *) &addr, addrlen);
}

void evx_listen_init(struct evx_listen *el,
                     void (*cb)(EV_P_ struct evx_listen *, int,
                                const struct sockaddr *, socklen_t))
{
  memset(el, 0, sizeof(*el));
  el->el_connect_cb = cb;
  ev_init(&el->el_io_w, &evx_listen_io_cb);
  el->el_io_w.fd = -1;
  el->el_nonblock = 1;
  el->el_cloexec = 1;
  el->el_reuseaddr = 1;
}

void evx_listen_set(struct evx_listen *el, int fd)
{
  if (el->el_addrlen == 0) {
    el->el_addrlen = sizeof(el->el_addr);
    getsockname(fd, (struct sockaddr *) &el->el_addr, &el->el_addrlen);
  }

  fd_set_nonblock(fd);
  fd_set_cloexec(fd);
  ev_io_set(&el->el_io_w, fd, EV_READ);
}

int evx_listen_bind(struct evx_listen *el, const struct sockaddr *addr,
                   socklen_t addrlen, int backlog)
{
  int fd = -1;

#if DEBUG
  char host[NI_MAXHOST], serv[NI_MAXSERV];
  int gni_rc = getnameinfo(addr, addrlen, host, sizeof(host),
                           serv, sizeof(serv), NI_NUMERICHOST|NI_NUMERICSERV);
  if (gni_rc != 0)
    ERROR("evx_listen_bind cannnot get nameinfo: %s\n",
          gai_strerror(gni_rc));
  else
    TRACE("evx_listen_bind host `%s', serv `%s'\n", host, serv);
#endif

  if (!(el->el_io_w.fd < 0)) {
    TRACE("evx_listen already bound, fd %d\n", el->el_io_w.fd);
    errno = EINVAL;
    goto err;
  }

  /* TODO Add flags SOCK_NONBLOCK and SOCK_NONBLOCK if available. */
  fd = socket(addr->sa_family, SOCK_STREAM, 0);
  if (fd < 0) {
    TRACE("evx_listen_bind cannot create socket: %s\n", strerror(errno));
    goto err;
  }

  if (el->el_reuseaddr) {
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
      TRACE("evx_listen_bind cannot set SO_REUSEADDR: %s\n", strerror(errno));
  }

  if (bind(fd, addr, addrlen) < 0) {
    TRACE("evx_listen_bind cannot bind: %s\n", strerror(errno));
    goto err;
  }

  if (listen(fd, backlog) < 0) {
    TRACE("evx_listen_bind cannot listen: %s\n", strerror(errno));
    goto err;
  }

  if (addrlen <= sizeof(el->el_addr)) {
    memcpy(&el->el_addr, addr, addrlen);
    el->el_addrlen = addrlen;
  }

  evx_listen_set(el, fd);

  return 0;

 err:
  if (!(fd < 0))
    close(fd);

  return -1;
}

/* Warning!  This function may block in getaddrinfo(). As with
   getaddrinfo(), either host or serv, but not both, may be NULL. */
int evx_listen_bind_name(struct evx_listen *el,
                        const char *host, const char *serv,
                        int family /* Use AF_*. AF_UNSPEC == 0 */, int backlog)
{
  struct addrinfo ai_hints = {
    .ai_family = family,
    .ai_socktype = SOCK_STREAM,
    .ai_flags = AI_PASSIVE, /* Ignored if host != NULL. */
  };
  struct addrinfo *ai, *ai_list = NULL;
  int rc = -1;

#define HOST (host != NULL ? host : "0.0.0.0")
#define SERV (serv != NULL ? serv : "0")

  TRACE("evx_listen_bind_name host `%s', service `%s'\n", HOST, SERV);

  int gai_rc = getaddrinfo(host, serv, &ai_hints, &ai_list);
  if (gai_rc != 0) {
    ERROR("cannot resolve host `%s', service `%s': %s\n",
          HOST, SERV, gai_strerror(gai_rc));
    /* TODO errno. */
    goto out;
  }

  for (ai = ai_list; ai != NULL; ai = ai->ai_next) {
    if (evx_listen_bind(el, ai->ai_addr, ai->ai_addrlen, backlog) == 0) {
      rc = 0;
      goto out;
    }
  }

  ERROR("cannot bind to host `%s', service `%s': %s\n",
        HOST, SERV, strerror(errno));

 out:
  freeaddrinfo(ai_list);

  return rc;
}

void evx_listen_start(EV_P_ struct evx_listen *el)
{
  ev_io_start(EV_A_ &el->el_io_w);
}

void evx_listen_stop(EV_P_ struct evx_listen *el)
{
  ev_io_stop(EV_A_ &el->el_io_w);
}

void evx_listen_close(struct evx_listen *el)
{
  if (!(el->el_io_w.fd < 0))
    close(el->el_io_w.fd);
  el->el_io_w.fd = -1;

  memset(&el->el_addr, 0, sizeof(el->el_addr));
  el->el_addrlen = 0;

  ev_init(&el->el_io_w, &evx_listen_io_cb);
}
