# event socks5 server Makefile

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

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo nah')

PROGRAM=esocks

CC=gcc

OBJ=evs_server.o evs_handlers.o evs_log.o evs_dns.o

SRC=evs_handlers.c evs_log.c evs_dns.c

ifeq ($(uname_S),Linux)
 DEFINES=-DAUTOCONF -DPOSIX -DUSG -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif
ifeq ($(uname_S),FreeBSD)
 DEFINES=-DAUTOCONF -DPOSIX -DSYSV -D_FREEBSD_C_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif
ifeq ($(uname_S),Darwin)
 DEFINES=-DAUTOCONF -DPOSIX -DSYSV -D_DARWIN_C_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif

LIBS=-levent -levent_core

CFLAGS=-std=c99 \
        -D_DEFAULT_SOURCE \
        -W \
        -Wstrict-prototypes \
        -Wmissing-prototypes \
        -Wno-sign-compare \
        -Wno-unused-parameter \
	-O3

$(PROGRAM): $(OBJ)
	$(E) "  LINK    " $@
	$(Q) $(CC) $(DEFINES) $(OBJ) $(LIBS) -o $@

clean:
	$(E) "  CLEAN "
	$(Q) rm -f *.o $(PROGRAM)

.c.o:
	$(E) "  CC      " $@
	$(Q) $(CC) $(CFLAGS) $(DEFINES) -c $*.c
