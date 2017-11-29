# event socks5 server Makefile

PROGRAM = esocks

CC = cc

LOADLIBS = -levent -levent_core

OBJ =  evsocks.o handlers.o

CFLAGS = -std=c99 -D_XOPEN_SOURCE=600 \
         -D_DEFAULT_SOURCE \
         -pedantic \
	 -Wall \
	 -W \
         -Wmissing-prototypes \
         -Wno-sign-compare \
         -Wno-unused-parameter

.o :
	${CC} -c  ${OBJ} -o $@

${PROGRAM} : ${OBJ}
	${CC} -o $@ ${OBJ} ${CFLAGS} ${LOADLIBS}

clean :
	${RM} *.o
