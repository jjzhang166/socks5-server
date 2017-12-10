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

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

PROGRAM=esocks

LIBS=-levent -levent_core

CC=gcc

OBJ=src/evsocks.o src/handlers.o src/slog.o

ifeq ($(uname_S),Linux)
 DEFINES=-DAUTOCONF -DPOSIX -DUSG -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif
ifeq ($(uname_S),FreeBSD)
 DEFINES=-DAUTOCONF -DPOSIX -DSYSV -D_FREEBSD_C_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif
ifeq ($(uname_S),Darwin)
 DEFINES=-DAUTOCONF -DPOSIX -DSYSV -D_DARWIN_C_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif

CFLAGS=-std=c99 \
        -D_DEFAULT_SOURCE \
        -pedantic \
        -Wall \
        -W \
        -Wstrict-prototypes \
        -Wmissing-prototypes \
        -Wno-sign-compare \
        -Wno-unused-parameter


$(PROGRAM): $(OBJ)
	$(E) "  LINK    " $@
	$(Q) $(CC) $(CFLAGS) $(DEFINES) -o $@ $(OBJ) $(LIBS)

clean:
	$(RM) src/*.o $(PROGRAM)

src/*.c.o:
	$(E) "  CC      " $@
	$(Q) ${CC} ${CFLAGS} ${DEFINES} -c $*.c
