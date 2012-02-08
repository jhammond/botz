#include <stdio.h>
#include "json.h"
#include "n_buf.h"

static const char *hello_world(void)
{
  return "Hello world!";
}

static int my_rand(void)
{
  return 9;
}

static unsigned long addr(void *data)
{
  return (unsigned long) data;
}

static void print(const char *s, struct json *j)
{
  char data[1];
  struct n_buf nb;

  n_buf_init(&nb, 4096);
  json_format(&nb, j, data);
  printf("%s: %s\n", s, nb.nb_buf);
  n_buf_destroy(&nb);
}

int main(int argc, char *argv[])
{
  int on = 1, off = 0;
  int i, v = 703, V = 704;

  struct json v0[] = {
    J_(string, F, &hello_world),
  };

  struct json v1[] = {
    J_(int, V, 1044),
  };

  struct json v2[] = {
    J_ARRAY,
    J_END,
  };

  struct json v3[] = {
    J_OBJECT,
      J_(string, V, "key"), J_(string, V, "value"),
    J_END,
  };

  struct json v4[102];

  v4[0] = J_ARRAY;

  for (i = 1; i < sizeof(v4) / sizeof(v4[0]) - 1; i++)
    v4[i] = J_(int, V, i);

  v4[i] = J_END;

  struct json v5[] = {
    J_ARRAY,
      J_OBJECT,
        J_PAIR("big_float", double, V, 1E43),
        J_PAIR("big_long", long, V, -1L << 43),
        J_PAIR("my_rand", int, F, &my_rand),
        J_PAIR("data_addr", ulong, D, &addr),
      J_END,
      J_(value, V, v0),
      J_ARRAY,
        J_ARRAY,
          J_(double, V, 0.789),
        J_END,
      J_END,
      J_(bool, V, on),
      J_(bool, P, &on),
      J_(value, V, v1),
      J_(int, V, v),
      J_(int, V, V),
      J_(int, P, &v),
      J_TRUE,
      J_FALSE,
      J_NULL,
    J_END,
  };

  struct json v6[] = {
    J_OBJECT,
      J_PAIR("key0", int, V, 0),
      J_PAIR("chars", string, V, "\b\f\n\r\t\"\'\\"),
    J_END,
  };

  struct json v7[] = {
    J_(raw, V, "{ \"key1\": 1, \"key2\": 2 }"),
  };

  struct json v8[] = {
    J_OBJECT,
      J_PAIR("on", bool, P, &on),
      J_PAIR("off", bool, P, &off),
    J_END,
  };

  struct json n0, n1, n2, *p0, *p1, *p2;
  n0 = J_NULL;
  p0 = &n0;
  n1 = J_(value, P, &p0);
  p1 = &n1;
  n2 = J_(value, P, &p1);
  p2 = &n2;

  struct json v9[] = {
    J_(value, P, &p2),
  };

#define print(v) print(#v, v)

  print(v0);
  print(v1);
  print(v2);
  print(v3);
  print(v4);
  print(v5);
  print(v6);
  print(v7);
  print(v8);
  off = 1, on = 0;
  print(v8);
  print(v9);

  return 0;
}
