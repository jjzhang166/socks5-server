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

PROGRAMS=test_hostname

LIBS=-levent -levent_core

SRC=../evs_handlers.c ../evs_dns.c ../evs_log.c tiny_test.c

CFLAGS=-std=c99 \
        -D_DEFAULT_SOURCE \
        -W

all: $(PROGRAMS)


test_hostname: test_hostname.o
	$(E) "  CC      " $@
	$(Q) $(CC) $(CFLAGS) test_hostname.o $(SRC) $(LIBS) -o $@ 

#test_hostname: test_hostname.o
#       $(E) "  CC      " $@
#       $(Q) $(CC) $(CFLAGS) test_hostname.o $(LIBS) -o $@

clean:
	$(Q) $(RM) $(PROGRAMS) *.o
