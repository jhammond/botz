#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include "json.h"
#include "n_buf.h"

static size_t json_string_format(struct n_buf *nb, const char *s, void *data)
{
  static char esc[] = {
    ['\0'] = '0',
    ['\a'] = 'a',
    ['\b'] = 'b',
    ['\t'] = 't',
    ['\n'] = 'n',
    ['\v'] = 'v',
    ['\f'] = 'f',
    ['\r'] = 'r',
    ['\"'] = '\"',
    ['\\'] = '\\',
  };

  /* This is probably broken for UNICODE. */
  size_t len = 0;

  len += n_buf_putc(nb, '\"');

  for (; *s != 0; s++) {
    int c = *(const unsigned char *) s;

    if (!isascii(c)) {
      len += n_buf_printf(nb, "\\u%04x", (unsigned int) c);
    } else if (c < sizeof(esc) && esc[c] != 0) {
      len += n_buf_putc(nb, '\\');
      len += n_buf_putc(nb, esc[c]);
    } else {
      len += n_buf_putc(nb, c);
    }
  }

  len += n_buf_putc(nb, '\"');

  return len;
}

static size_t json_bool_format(struct n_buf *nb, int v, void *data)
{
  return n_buf_printf(nb, v ? "true" : "false");
}

static size_t json_format_r(struct n_buf *nb, struct json **j,
                            void *data, int in_object, int in_array);

static size_t json_value_format(struct n_buf *nb, struct json *j,
                                void *data)
{
  return json_format_r(nb, &j, data, 0, 0);
}

static size_t json_format_1(struct n_buf *nb, struct json *j, void *data)
{
  switch (j->j_type & ~3) {
#define X(type, c_type, fmt, func)                                \
    case J_ ## type ## _V: {                                      \
      typeof(c_type) cv;                                          \
      switch (j->j_type & 3) {                                    \
      case 0:                                                     \
        cv = (j->j_u.u_ ## type ## _V);                           \
        break;                                                    \
      case 1:                                                     \
        cv = *(j->j_u.u_ ## type ## _P);                          \
        break;                                                    \
      case 2:                                                     \
        cv = (*(j->j_u.u_ ## type ## _F))();                      \
        break;                                                    \
      case 3:                                                     \
        cv = (*(j->j_u.u_ ## type ## _D))(data);                  \
        break;                                                    \
      }                                                           \
      if ((func) != NULL)                                         \
        return ((size_t (*)(struct n_buf *, c_type, void *))      \
               (func))(nb, cv, data);                             \
      else                                                        \
        return n_buf_printf(nb, fmt, cv);                         \
    }
    JSON_TYPES
#undef X
  }

  return 0;
}

static size_t json_format_r(struct n_buf *nb, struct json **j,
                            void *data, int in_object, int in_array)
{
  size_t len = 0;
  int i;

  for (i = 0; (*j)->j_type != J_end_V; (*j)++) {

    if (i > 0) {
      if (in_array)
        len += n_buf_putc(nb, ',');
      else if (in_object)
        len += n_buf_putc(nb, i % 2 ? ':' : ',');
      else
        break;
    }

    switch ((*j)->j_type) {
    case J_init_V:
      ((*j)->j_u.u_init_V)();
      break;
    case J_array_V:
      (*j)++;
      len += n_buf_putc(nb, '[');
      len += json_format_r(nb, j, data, 0, 1);
      len += n_buf_putc(nb, ']');
      break;
    case J_object_V:
      (*j)++;
      len += n_buf_putc(nb, '{');
      len += json_format_r(nb, j, data, 1, 0);
      len += n_buf_putc(nb, '}');
      break;
    default:
      len += json_format_1(nb, *j, data);
      break;
    }

    i += ((*j)->j_type != J_init_V);
  }

  return len;
}

ssize_t json_format(struct n_buf *nb, struct json *j, void *data)
{
  size_t len = json_format_r(nb, &j, data, 0, 0);

  n_buf_put0(nb);

  return len;
}
