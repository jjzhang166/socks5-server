#ifndef EVFUNC_H
#define EVFUNC_H

#include <arpa/inet.h>

extern unsigned char * get_socks_header(void );

static void reader_func(struct bufferevent *bev, void *ctx);

static void event_func(struct bufferevent *bev, short what, void *ctx);

static void syntax(void);

static void send_reply(struct bufferevent *bev, uint8_t reply);

static void handle_connect(struct bufferevent *bev, unsigned char * buffer, ev_ssize_t evsize);

static void handle_bind(unsigned char * buffer, ev_ssize_t evsize);

static void handle_udpassoc(void);

/* handle_spec inspects address types. */
static void handle_addrspec(unsigned char * buffer);

static const char * debug_ntoa(uint32_t address);

struct addrspec {
  const char *address;
  uint16_t port;
};

#endif
