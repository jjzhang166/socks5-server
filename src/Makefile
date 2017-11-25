# event socks5 server Makefile

PROGRAM = esocks

CC = cc

LDLIBS =

SRC = evsocks.c internal.c handlers.c

HDR = evs.hks.h internal.h handlers.h

OBJ = internal.o handlers.o evsocks.o 

CFLAGS = -std=c99 -D_XOPEN_SOURCE=600 \
         -D_DEFAULT_SOURCE \
         -pedantic \
	 -Wall \
	 -W \
         -Wmissing-prototypes \
         -Wno-sign-compare \
         -Wno-unused-parameter \
	 -levent

all : ${PROGRAM}

.o :
	${CC} -c -o $@

${PROGRAM} : ${OBJ}
	${CC} ${CFLAGS} -o $@ ${OBJ}

clean :
	${RM} *.o esocks

showall :
	@echo ${EXE}
