#include <event2/bufferevent.h>
#include <event2/util.h>

static void syntax(void);

static void event_func(struct bufferevent *bev, short what, void *ctx);

static void accept_func(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *a, int slen, void *p);

static void socks_init_func(struct bufferevent *bev, void *ctx);

static void async_read_func(struct bufferevent *bev, void *ctx);

/* listens to target address and writes data back to clietns */
static void async_write_func(struct bufferevent *bev, void *ctx);

static void async_handle_read_from_target(struct bufferevent *bev, void *ctx);

static void handle_perpetrators(struct bufferevent *bev);

static void signal_func(evutil_socket_t sig_flag, short what, void *ctx);
