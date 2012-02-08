#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "botz.h"
#include "json.h"
#include "trace.h"
#include "n_buf.h"

static char hostname[256];

static const char *get_date(void)
{
  time_t now = time(0);
  return ctime(&now);
}

static double get_uptime(void)
{
  double uptime = -1;
  FILE *file = fopen("/proc/uptime", "r");
  if (file != NULL) {
    fscanf(file, "%lf", &uptime);
    fclose(file);
  }
  return uptime;
}

static double loadavg[3];

static int loadavg_init(void)
{
  return getloadavg(loadavg, 3);
}

static struct json loadavg_json[] = {
  J_ARRAY,
  J_INIT(&loadavg_init),
  J_(double, P, &loadavg[0]),
  J_(double, P, &loadavg[1]),
  J_(double, P, &loadavg[2]),
  J_END,
};

static char exe[256], cwd[256];
static struct rusage ru;
static double ru_utime, ru_stime;

static int self_init(void)
{
  readlink("/proc/self/exe", exe, sizeof(exe));
  readlink("/proc/self/cwd", cwd, sizeof(cwd));
  getrusage(RUSAGE_SELF, &ru);
  ru_utime = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec * 1e-6;
  ru_stime = ru.ru_stime.tv_sec + ru.ru_stime.tv_usec * 1e-6;

  return 0;
}

static struct json self_json[] = {
  J_OBJECT,
  J_INIT(&self_init),
  J_PAIR("name", string, P, (const char **) &program_invocation_short_name),
  J_PAIR("exe", string, V, exe),
  J_PAIR("cwd", string, V, cwd),
  J_PAIR("pid", int, F, &getpid),
  J_PAIR("uid", uint, F, &getuid),
  J_PAIR("utime", double, P, &ru_utime),
  J_PAIR("stime", double, P, &ru_stime),
  J_PAIR("maxrss", long, P, &ru.ru_maxrss),
  J_PAIR("nsignals", long, P, &ru.ru_nsignals),
  J_PAIR("nvcsw", long, P, &ru.ru_nvcsw),
  J_PAIR("nivcsw", long, P, &ru.ru_nivcsw),
  J_END,
};

static struct json outage_json[] = {
  J_OBJECT,
  J_PAIR("ib_hca", bool, V, 1),
  J_PAIR("nvidia_dbe", bool, V, 1),
  J_PAIR("/home1", bool, V, 1),
  J_PAIR("/sge_common", bool, V, 1),
  J_PAIR("/corral", bool, V, 1),
  J_PAIR("/exp1", bool, V, 1),
  J_PAIR("/tmp", long, V, 56032),
  J_PAIR("/dev/shm", long, V, 3604),
  J_END,
};

static struct json status_json[] = {
  J_OBJECT,
  J_PAIR("hostname", string, V, hostname),
  J_PAIR("date", string, F, get_date),
  J_PAIR("jobid", int, V, 2350105),
  J_PAIR("start_time", long, V, 1328734787), /* XXX */
  J_PAIR("end_time", long, V, 0),
  J_PAIR("uptime", double, F, get_uptime),
  J_PAIR("loadavg", value, V, loadavg_json),
  J_PAIR("outage", value, V, outage_json),
  J_PAIR("self", value, V, self_json),
  J_END,
};

int main(int argc, char *argv[])
{
  const char *addr = argc < 1 ? "localhost" : argv[1];
  const char *port = argc < 2 ? "9901" : argv[2];
  struct botz_listen bl;
  struct evx_listen *el;

  gethostname(hostname, sizeof(hostname) - 1);

  if (botz_listen_init(&bl, 64) < 0)
    FATAL("cannot initialize listener: %s\n", strerror(errno));

  if (botz_set_json(&bl, "/loadavg", loadavg_json, NULL) < 0)
    FATAL("cannot set resource `%s': %s\n", "/loadavg", strerror(errno));

  if (botz_set_json(&bl, "/status", status_json, NULL) < 0)
    FATAL("cannot set resource `%s': %s\n", "/status", strerror(errno));

  el = &bl.bl_listen;

  if (evx_listen_bind_name(el, addr, port, 0, 128) < 0)
    FATAL("cannot bind to interface `%s', service `%s': %s\n",
          addr, port, strerror(errno));

  evx_listen_start(EV_DEFAULT_ el);

  ev_run(EV_DEFAULT_ 0);

  return 0;
}
