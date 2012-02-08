#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "evx.h"
#include "trace.h"

void el_connect_cb(EV_P_ struct evx_listen *el, int fd,
                   const struct sockaddr *addr, socklen_t addrlen)
{
  char *msg = "Hello world!\n";

  if (write(fd, msg, strlen(msg)) < 0)
    ERROR("cannot write: %s\n", strerror(errno));

  close(fd);
}

int main(int argc, char *argv[])
{
  const char *addr = argc < 1 ? "localhost" : argv[1];
  const char *port = argc < 2 ? "9901" : argv[2];
  struct evx_listen el;

  evx_listen_init(&el, &el_connect_cb);

  if (evx_listen_bind_name(&el, addr, port, 0, 128) < 0)
    FATAL("ev_listen_bind_name failed: %s\n", strerror(errno));

  evx_listen_start(EV_DEFAULT_ &el);

  ev_run(EV_DEFAULT_ 0);

  return 0;
}
