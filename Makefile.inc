APP_DIR = ..
APP_LIB = ${APP_DIR}/libsocks.a

LIB_EVENT = -levent

IMPL_CFLAGS = -std=c99 -D_XOPEN_SOURCE=600 \
	      -D_DEFAULT_SOURCE \
      	      -g -I${APP_DIR}/lib \
	      -pedantic \
	      -Wall \
	      -W \
              -Wmissing-prototypes \
              -Wno-sign-compare \
              -Wno-unused-parameter

CFLAGS = ${IMPL_CFLAGS}

IMPL_THREAD_FLAGS = -pthread

IMPL_LDLIBS = ${APP_LIB} -lm

LDLIBS = ${IMPL_LDLIBS}

RM = rm -f