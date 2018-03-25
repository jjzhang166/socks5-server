/* 
 * Watch out - this is a work in progress!
 *
 * Use of this source code is governed by a
 * license that can be found in the LICENSE file.
 * 
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

// Sorry if don't have this,
#include <netinet/tcp.h>

#include <sys/stat.h>
#include <signal.h>

#include <event2/listener.h>

#include "evs_internal.h"
#include "evs_log.h"
#include "evs_helper.h"
#include "evs_lru.h"
#include "evs_encryptor.h"
#include "evs_server.h"

static int status;
static struct event_base *base;
static void signal_func(evutil_socket_t sig_flag, short what, void *ctx);
static void listen_func(evutil_socket_t, short, void*);
static void accept_func(evutil_socket_t, short, void*);
static void socks_initcb(struct bufferevent *bev, void *ctx);
static void eventcb(struct bufferevent *bev, short what, void *ctx);
static void resolvecb(socks_name_t *s);
static void destroycb(struct bufferevent *bev);

const char *
_getprogname(void)
{
  return "server";
}

void
run_srv(srv_conf_t *conf)
{
  struct event *signal_event;
  struct event *listen_event;
  struct sockaddr_in sin;
  unsigned long to = conf->timeout;
  struct timeval timeout = {to, 0};  
  int fd;
  int optval = 5;  
  u16 port;
  void *ctx;
  
  memset(&sin, 0, sizeof(sin));
  port = conf->srv_port;

  sin.sin_port = htons(port);
  sin.sin_family = AF_INET;

  if (evutil_inet_pton(AF_INET, conf->srv_addr, (struct sockaddr*)&sin.sin_addr) <0)
    log_errx(1, "inet_pton");

  // create fd
  fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
  if (fd == -1)
    goto err;
  
  if (setsockopt(fd, SOL_TCP, TCP_FASTOPEN, (void*)&optval, sizeof(optval)) <0)
    log_errx(1, "setsockopt, probably caused by TCP_FASTOPEN");
  
  if (evutil_make_listen_socket_reuseable(fd) <0)
    goto err;
  if (evutil_make_listen_socket_reuseable_port(fd) <0)
    goto err;
  if (evutil_make_tcp_listen_socket_deferred(fd) <0)
    goto err;
  if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) <0)
    goto err;
  if (listen(fd, 1024) <0)
    goto err;
  
  base = event_base_new();
  signal_event = event_new(base, SIGINT,
			   EV_SIGNAL|EV_PERSIST, signal_func, (void*)base);
  event_add(signal_event, NULL);

  // set callback func
  listen_event = event_new(base, fd,
			   EV_READ|EV_WRITE|EV_PERSIST, listen_func, (void*)&timeout);
  event_add(listen_event, &timeout);
  
  event_base_dispatch(base);
  event_base_free(base);
  exit(0);

 err:
  evutil_closesocket(fd);
  log_errx(1, "evutil_make_* or bind cause some error");  
}

static void
accept_func(evutil_socket_t fd, short what, void *ctx)
{
  struct bufferevent *bev, *partner;
  
  bev = bufferevent_socket_new(base, fd,
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  partner = bufferevent_socket_new(base, -1, 
				   BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  assert(bev && partner);
  bufferevent_setcb(bev, socks_initcb, NULL, eventcb, partner);
  bufferevent_enable(bev, EV_READ|EV_WRITE);  
}

static void
listen_func(evutil_socket_t fd, short what, void *ctx)
{
  struct timeval *t = ctx;
  int new_fd;
  int err;
  socklen_t addrlen;
  // keep idling state
  log_info("%ld sec elapsed", t->tv_sec);

  while(1) {
    struct sockaddr_storage ss;
    addrlen = sizeof(ss);
    new_fd = accept(fd, (struct sockaddr*)&ss, &addrlen);
    if (new_fd <0)
      break;
    if (fcntl(new_fd, F_SETFL, O_NONBLOCK) == -1)
      log_errx(1, "fcntl");
    
    accept_func(new_fd, what, ctx);
  }
  err = evutil_socket_geterror(fd);
  log_info("error=%d", err);
  return;
}

static void
eventcb(struct bufferevent *bev, short what, void *ctx)
{
  log_info("eventcb %d", what);
}

static void
socks_initcb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *targ = ctx;
  struct evbuffer *src = bufferevent_get_input(bev);
  struct sockaddr_in sin;  
  struct sockaddr_in6 sin6;  
  size_t buf_size = evbuffer_get_length(src);
  socks_name_t *name;
  int buflen;
  u8 v4_buf[4];
  u8 portbuf[2];
  u8 v6_buf[16];
  u8 port_buf[2];  
  u8 buf[buf_size]; /* fill buffer later */
  u8 abuf[128];
  u8 domain_len;
  u8 port;

  evbuffer_copyout(src, buf, buf_size);

  if (buf[0] == 5) {
    status = SINIT;
    evbuffer_drain(src, buf_size);
    switch(buf[3]) {
    case IPV4:
      memcpy(v4_buf, buf+4, sizeof(v4_buf));
      if (evutil_inet_pton(AF_INET, abuf, &sin.sin_addr) < 1) {
	      log_err("failed to resolve addr");
	      destroycb(bev);
	      return;
      }
      struct sockaddr_in sin;
      memset(&sin, 0, sizeof(sin));
      sin.sin_family = AF_INET;
      sin.sin_port = htons(port);
      if (bufferevent_socket_connect(targ,
				     (struct sockaddr*)&sin, sizeof(sin)) != 0) {
	log_err("connect: failed to connect");
	destroycb(bev);
	      return;
      }
      log_debug(DEBUG, "v4 connect immediate");
      status = SCONNECTED;
      break;
      case IPV6:
	if (evutil_inet_ntop(AF_INET6, buf + 4, abuf,
			     SOCKS_INET6_ADDRSTRLEN) == NULL) {
	  log_err("invalid v6 address");
	  destroycb(bev);
	  return;
	}
	/* Extract 16 bytes address */
	if (evutil_inet_pton(AF_INET6, abuf, &sin6.sin6_addr) < 1){
	  log_err("v6: failed to resolve addr");
	  destroycb(bev);
	  return;
	}	
    case DOMAINN:
      domain_len = (u8) buf[4];	  
      buflen = (int)domain_len + 5;
      /* extract 2 bytes port */      
      memcpy(portbuf, buf + buflen, 2);
      port = portbuf[0]<<8 | portbuf[1];
      name = malloc(sizeof(socks_name_t));
      if (name == NULL) {
	log_err("malloc");
      }
      name->host = buf + 5;
      name->hlen = domain_len;
      name->bev = bev;
      name->family = AF_INET;
      name->port = htons(port);
      
      (void)resolvecb(name);
      
	  if (status == DNS_OK) {
	    log_debug(DEBUG, "dns_ok");
	    evbuffer_drain(src, buf_size);
	    struct sockaddr_in *sin_p = (struct sockaddr_in*)name->addrs[0].sockaddr;
	    sin_p->sin_family = AF_INET;
	    sin_p->sin_port = htons(port);
	    if (evutil_inet_ntop(AF_INET, (struct sockaddr*)&sin_p->sin_addr,
	    			 abuf, sizeof(abuf))
	    	== NULL) {
	      log_err("failed to resolve host");
	      destroycb(bev);
	      return;
	    }
	    log_info("connecting to %s:%d", abuf, port);
	    if (bufferevent_socket_connect(targ,
				   (struct sockaddr*)sin_p, name->addrs[0].socklen) != 0) {
	  	log_err("connect: failed to connect");
	  	destroycb(bev);
	  	return;
	    }
	    status = SCONNECTED;
	  }
    }
  }
}

static void
resolvecb(socks_name_t *s)
{
//  if (resolve_host(s, &dns_c) == 0) {
//    status = DNS_OK;
//  } else {
//    destroycb(s->bev);
//    return;
//  }
}

static void
signal_func(evutil_socket_t sig_flag, short what, void *ctx)
{
  struct event_base *base = ctx;
  struct timeval delay = {1, 0};
  int sec = 1;
  
  log_info(
       "Caught an interupt signal; exiting cleanly in %d second(s)", sec);
  
  event_base_loopexit(base, &delay);  
}

static void
destroycb(struct bufferevent *bev)
{
  status = 0;
  
  log_info("destroyed");

  /* Unset all callbacks */
  bufferevent_free(bev);
}
