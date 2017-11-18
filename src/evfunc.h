#ifndef EVFUNC_H
#define EVFUNC_H

static const char * get_socks_header(void );

static void reader_func(struct bufferevent *bev, void *ctx);

static void event_func(struct bufferevent *bev, short what, void *ctx);

static void syntax(void);

static void send_reply(struct bufferevent *bev, uint8_t reply);

static void handle_connect(struct bufferevent *bev, unsigned char * buffer, ev_ssize_t evsize);

static void handle_bind(unsigned char * buffer, ev_ssize_t evsize);

static void handle_udpassoc(void);

static const char * ntov4a(uint32_t address);

struct addrspec {
  const char *domain;
  unsigned long s_addr;
  unsigned char *_s6_addr;
  uint16_t port; /* load with shift operator. otherwise endianness will be messed.. */
};

/* handle_spec inspects address types. */
struct addrspec * handle_addrspec(unsigned char * buffer);

#endif
