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

OBJ =  evs_server.o evs_log.o evs_helper.o evs_bst.o

SRC =  evs_log.c evs_helper.c evs_server.c evs_bst.c

ifeq ($(DEBUG), yes)
	DE = -DDEBUG=1 -g -DFAST_OPEN # debug and ready for gdb and fast-open
endif

LIBS = -levent -levent_core

CFLAGS = -std=c99 \
         -D_DEFAULT_SOURCE \
         -W \
	 -O2 \
	 $(DE)


$(PROGRAM): $(OBJ)
	$(E) "  LINK    " $@
	$(Q) $(CC)  $(CFLAGS) $(SRC) $(LIBS) -o $@


.c.o:
	$(Q) $(CC) -c $(SRC)


clean:
	$(E) "  CLEAN "
	$(Q) rm -f *.o $(PROGRAM)
