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

PROGRAMS = ./test_bst ./test_enc ./test_hostname ./test_2lru

LIBS = -levent -levent_core

CFLAGS = -std=c99 \
         -D_DEFAULT_SOURCE \
         -W \
	 -g \
	 -DDEBUG

all: $(PROGRAMS)

t:
	$(Q)for t in $(PROGRAMS); do\
		$$t;\
	done

test_hostname: test_hostname.o
	$(Q) $(CC) $(CFLAGS) -DSOCKS_HAVE_INET6 \
	../evs_helper.c ../evs_lru.c ../evs_log.c ../evs_encryptor.c \
	tiny_test.c test_hostname.c \
	$(LIBS) -o $@

test_bst: test_bst.o
	$(Q) $(CC) $(CFLAGS) \
	../evs_bst.c ../evs_log.c test_bst.c tiny_test.c \
	$(LIBS) -o $@

test_enc: test_enc.o
	$(Q) $(CC) $(CFLAGS) \
	../evs_encryptor.c ../evs_log.c test_enc.c tiny_test.c \
	$(LIBS) -o $@

test_2lru: test_2lru.o
	$(Q) $(CC) $(CFLAGS) \
	../evs_log.c ../evs_lru.c tiny_test.c test_2lru.c \
	$(LIBS) -o $@

clean:
	$(Q) $(RM) $(PROGRAMS) *.o
