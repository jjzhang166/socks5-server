# Makefile for test

# Make the build silent
V =

ifeq ($(strip $(V)),)
        E = @echo
        Q = @
else
        E = @\#
        Q =
endif
export E Q

PROGRAMS=test_hostname test_bst

LIBS=-levent -levent_core

SRC= tiny_test.c

CFLAGS=-std=c99 \
        -D_DEFAULT_SOURCE \
        -W \
	-g


all: $(PROGRAMS)

test_hostname: test_hostname.o
	$(E) "  CC      " $@
	$(Q) $(CC) $(CFLAGS) -DDEBUG \
	-DSOCKS_HAVE_INET6 \
	test_hostname.o ../evs_log.c ../evs_helper.c tiny_test.c $(LIBS) -o $@ 

test_bst: test_bst.o
	$(E) "  CC      " $@
	$(Q) $(CC) $(CFLAGS) -DDEBUG \
	test_bst.o ../evs_log.c ../evs_helper.c ../evs_bst.c tiny_test.c $(LIBS) -o $@

clean:
	$(Q) $(RM) $(PROGRAMS) *.o
