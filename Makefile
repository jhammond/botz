DEBUG = 0

CC = gcc
CPPFLAGS = -D_GNU_SOURCE \
           -DDEBUG=$(DEBUG) \
           -I/usr/local/include

CFLAGS = -Wall -Werror -g
LDFLAGS = -L/usr/local/lib -lev

OBJS = botz.o botzd.o evx_listen.o hash.o json.o n_buf.o test_evx_listen.o test_json.o

all: botzd test_evx_listen test_json tiny_botzd

botzd: botzd.o botz.o hash.o n_buf.o evx_listen.o json.o

test_evx_listen: test_evx_listen.o evx_listen.o

test_json: test_json.o n_buf.o json.o

tiny_botzd: tiny_botzd.o botz.o hash.o n_buf.o evx_listen.o json.o

-include $(OBJS:%.o=.%.d)

%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $*.c -o $*.o
	$(CC) -MM $(CFLAGS) $(CPPFLAGS) $*.c > .$*.d

.PHONY: clean
clean:
	rm -f botzd test_evx_listen test_json tiny_botzd *.o
