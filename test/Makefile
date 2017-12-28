# Makefile for test

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

PROGRAMS=test_hostname

LIBS=-levent -levent_core

SRC=../handlers.c ../async_dns.c ../slog.c tiny_test.c

CFLAGS=-std=c99 \
        -D_DEFAULT_SOURCE \
        -W

all: $(PROGRAMS)

test_hostname: test_hostname.o
	$(E) "  CC      " $@
	$(Q) $(CC) $(CFLAGS) -o $@ test_hostname.o $(LIBS) $(SRC)
	@echo `./test_hostname`

clean:
	$(RM) $(PROGRAMS) *.o
