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

PROGRAM = esocks

CC = gcc

OBJ = evs_handlers.o evs_log.o evs_dns.o evs_server.o 

SRC = evs_handlers.c evs_log.c evs_dns.c

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo nah')

ifeq ($(uname_S),Linux)
DEFINES = -DGETADDRINFO_A
endif

# ifeq ($(uname_S),Linux)
#  DEFINES =
# endif
# ifeq ($(uname_S),FreeBSD)
#  DEFINES =
# endif
# ifeq ($(uname_S),Darwin)
#  DEFINES =
# endif

ifeq ($(DEBUG), yes)
	DE = -DDEBUG=1
endif

LIBS = -levent -levent_core

CFLAGS = -std=c99 \
         -D_DEFAULT_SOURCE \
         -W \
         -Wstrict-prototypes \
         -Wmissing-prototypes \
         -Wno-sign-compare \
         -Wno-unused-parameter \
	 -O3

$(PROGRAM): $(OBJ)
	$(E) "  LINK    " $@
	$(Q) $(CC) $(OBJ) $(LIBS) -o $@

clean:
	$(E) "  CLEAN "
	$(Q) rm -f *.o $(PROGRAM)

.c.o:
	$(E) "  CC      " $@
	$(Q) $(CC) $(DEFINES) $(CFLAGS) $(DEFINES) $(DE) -c $*.c
