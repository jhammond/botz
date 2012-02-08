#ifndef _JSON_H_
#define _JSON_H_

#define JSON_TYPES                                           \
  X(init,   int (*)(void),      "%p",   NULL)                \
  X(end,    int,                "%d",   NULL)                \
  X(object, int,                "%d",   NULL)                \
  X(array,  int,                "%d",   NULL)                \
  X(value,  struct json *,      "%p",   &json_value_format)  \
  X(string, const char *,       "%s",   &json_string_format) \
  X(bool,   int,                "%d",   &json_bool_format)   \
  X(int,    int,                "%d",   NULL)                \
  X(long,   long,               "%ld",  NULL)                \
  X(llong,  long long,          "%lld", NULL)                \
  X(uint,   unsigned int,       "%u",   NULL)                \
  X(ulong,  unsigned long,      "%lu",  NULL)                \
  X(ullong, unsigned long long, "%llu", NULL)                \
  X(double, double,             "%f",   NULL)                \
  X(raw,    const char *,       "%s",   NULL)

enum {
#define X(type, ...) \
  J_ ## type ## _V, \
  J_ ## type ## _P, \
  J_ ## type ## _F, \
  J_ ## type ## _D,
  JSON_TYPES
#undef X
};

struct json {
  int j_type;
  union {
#define X(type, c_type, ...)                  \
    typeof(c_type) u_ ## type ## _V,          \
                  *u_ ## type ## _P,          \
                 (*u_ ## type ## _F)(void),   \
                 (*u_ ## type ## _D)(void *);
    JSON_TYPES
#undef X
  } j_u;
};

#define J_(type,M,val) (struct json) {  \
    .j_type = J_ ## type ## _ ## M,          \
    .j_u.u_ ## type ## _ ## M = (val),       \
  }

#define J_INIT(func) J_(init, V, func)
#define J_EXIT(func) J_(init, V, func)
#define J_END J_(end, V, 0)
#define J_OBJECT J_(object, V, 0)
#define J_PAIR(str, type, M, val) J_(string, V, (str)), J_(type, M, (val))
#define J_ARRAY J_(array, V, 0)
#define J_TRUE J_(bool, V, 1)
#define J_FALSE J_(bool, V, 0)
#define J_NULL J_(raw, V, "null")

struct n_buf;

ssize_t json_format(struct n_buf *nb, struct json *j, void *data);

#endif
