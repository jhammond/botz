#include <string.h>
#include <unistd.h>
#include "botz.h"
#include "json.h"
#include "trace.h"

int main(int argc, char *argv[])
{
  struct botz_listen bl;
  struct evx_listen *el;

  int up = 1;
  unsigned long nr_clients = 1000000;

  struct json status_json[] = {
    J_OBJECT,
    J_PAIR("name", string, V, "tiny_botzd"),
    J_PAIR("up", bool, P, &up),
    J_PAIR("nr_clients", ulong, P, &nr_clients),
    J_END,
  };

  if (botz_listen_init(&bl, 64) < 0)
    FATAL("cannot initialize listener: %s\n", strerror(errno));

  if (botz_set_json(&bl, "/status", status_json, NULL) < 0)
    FATAL("cannot set resource `/status': %s\n", strerror(errno));

  el = &bl.bl_listen;

  if (evx_listen_bind_name(el, "localhost", "9901", 0, 128) < 0)
    FATAL("cannot bind to interface `localhost', service `9901': %s\n",
          strerror(errno));

  evx_listen_start(EV_DEFAULT_ el);

  ev_run(EV_DEFAULT_ 0);

  return 0;
}
