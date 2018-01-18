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

OBJ =  evs_server.o evs_log.o evs_helper.o

ifeq ($(DEBUG), yes)
	DE = -DDEBUG=1 -g # debug and ready for gdb
endif

LIBS = -levent -levent_core

CFLAGS = -std=c99 \
         -D_DEFAULT_SOURCE \
         -W \
         -Wstrict-prototypes \
         -Wmissing-prototypes \
         -Wno-sign-compare \
         -Wno-unused-parameter \
	 -O3 \
	 $(DE)


$(PROGRAM): $(OBJ)
	$(E) "  LINK    " $@
	$(Q) $(CC) $(OBJ) $(LIBS) -o $@

.c.o:
	$(E) "  CC      " $@
	$(Q) $(CC) $(CFLAGS) -c evs_server.c evs_log.c evs_helper.c

clean:
	$(E) "  CLEAN "
	$(Q) rm -f *.o $(PROGRAM)

