# event socks5 server Makefile

IMPL_CFLAGS = -std=c99 -D_XOPEN_SOURCE=600 \
	      -D_DEFAULT_SOURCE \
	      -pedantic \
	      -Wall \
	      -W \
              -Wmissing-prototypes \
              -Wno-sign-compare \
              -Wno-unused-parameter

CFLAGS = ${IMPL_CFLAGS}

IMPL_THREAD_FLAGS = -pthread

LDLIBS = /usr/local/lib/libevent.a

GEN_EXE =

APP_LIB = libsocks.a

LINUX = evsocks

EXE = ${GEN_EXE} ${LINUX}

all : ${EXE}

allgen : ${APP_LIB} ${GEN_EXE}

${APP_LIB} : internal.c
	${CC} -c -g ${CFLAGS} internal.c
	${ar} rs ${APP_LIB} *.o

# clean : ${RM} ${EXE} *.o

showall :
	@echo ${EXE}

# include ../Makefile.inc
# 
# GEN_EXE =
# 
# LINUX = evsocks
# 
# EXE = ${GEN_EXE} ${LINUX}
# 
# all : ${EXE}
# 
# allgen : ${GEN_EXE}
# 
# LDLIBS = ${IMPL_LDLIBS} -levent
# 
# clean : ${RM} ${EXE} *.o
# 
# showall :
# 	@echo ${EXE}
