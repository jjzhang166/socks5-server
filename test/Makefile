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

SRC=../handlers.c ../evdns.c ../slog.c

CFLAGS=-std=c99 \
        -D_DEFAULT_SOURCE \
        -W

all: $(PROGRAMS)

test_hostname: test_hostname.o
	$(E) "  CC      " $@
	$(Q) $(CC) $(CFLAGS) $(LIBS) $(SRC) test_hostname.o -o $@
	@echo 'updated' `date` >> test-result.txt
	@echo `./test_hostname >> test-result.txt`
	@echo '' >> test-result.txt

clean:
	$(RM) $(PROGRAMS) *.o
