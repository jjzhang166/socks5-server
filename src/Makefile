# event socks5 server Makefile

PROGRAM = esocks

CFLAGS = -std=c99 -D_XOPEN_SOURCE=600 \
         -D_DEFAULT_SOURCE \
         -pedantic \
	 -Wall \
	 -W \
         -Wmissing-prototypes \
         -Wno-sign-compare \
         -Wno-unused-parameter \
	 -levent

CC = cc

LDLIBS =

OBJ = evsocks.o internal.o

all : ${PROGRAM}

.o :
	${CC} -c ${CFLAGS} -o $@

${PROGRAM} : ${OBJ}
	${CC} ${CFLAGS} -o $@ ${OBJ}

clean :
	${RM} *.o

showall :
	@echo ${EXE}
