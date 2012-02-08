#ifndef _EV_LISTEN_H_
#define _EV_LISTEN_H_
#include <ev.h>
#include <sys/socket.h>

struct evx_listen {
  void (*el_connect_cb)(EV_P_ struct evx_listen *, int fd,
                        const struct sockaddr *, socklen_t);
  /* void (*el_error_cb)(EV_P_ struct ev_listener *, int errno); */
  struct ev_io el_io_w;
  struct sockaddr_storage el_addr;
  socklen_t el_addrlen;
  /* All of the bits below are set by default (in init).  cloexec and
     nonblock are always set on the listening socket, these two bits
     only control what happens to sockets returned by accept().
     reuseaddr only applies to the listening socket. */
  unsigned int el_cloexec:1, el_nonblock:1, el_reuseaddr:1;
};

void evx_listen_init(struct evx_listen *el,
                     void (*cb)(EV_P_ struct evx_listen *, int,
                                const struct sockaddr *, socklen_t));

/* evx_listen_set: el must be initialized but stopped. fd must be bound
   and listening.  Sets el_addr if not already set. */
void evx_listen_set(struct evx_listen *el, int fd);

/* evx_listen_bind: Creates a socket, sets flags, binds it to addr, and calls listen().
   el must be initialized, stopped, and not already bound().  */
int evx_listen_bind(struct evx_listen *el, const struct sockaddr *addr,
                    socklen_t addrlen, int backlog);

/* evx_listen_bind_name: Looks up host:serv using getaddrinfo(), tries
   to bind to the addresses returned, stopping on success.  Warning!
   This function may block in getaddrinfo(). As with getaddrinfo(),
   either host or serv, but not both, may be NULL. */
int evx_listen_bind_name(struct evx_listen *el,
                         const char *host, const char *serv,
                         int family /* Use AF_*. AF_UNSPEC == 0 */, int backlog);

void evx_listen_start(EV_P_ struct evx_listen *el);

void evx_listen_stop(EV_P_ struct evx_listen *el);

/* el_listen_close: closes socket, clears el_addr, reinitializes
   el_io_w.  el must be stopped. */
void evx_listen_close(struct evx_listen *el);

#endif
