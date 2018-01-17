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

OBJ =  evs_server.o evs_log.o

ifeq ($(uname_S),Linux)
DEFINES = -DGETADDRINFO_A
endif

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


evs_server.o:
	$(E) "  CC      " $@
	$(Q) $(CC) $(CFLAGS) -c evs_server.c evs_log.c
