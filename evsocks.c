/* 
 * Use of this source code is governed by a
 * license that can be found in the LICENSE file.
 *
 * Simple proxy server with Libevent 
 * 
 *
*/

#if defined(__APPLE__) && defined(__clang__)
#program clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <assert.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcip.h>
#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <signal.h>
#include <sys/stat.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include "internal.h"
#include "slog.h"

/* store a comma separated character */
const char *auth = NULL;

static struct event_base *base;

/* status holds current eventbuffer's status */
static int status;
static void syntax(void);
static void event_func(struct bufferevent *bev, short what, void *ctx);
static void accept_func(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *a, int slen, void *p);
static void socks_init_func(struct bufferevent *bev, void *ctx);
static void async_read_func(struct bufferevent *bev, void *ctx);
static void async_write_func(struct bufferevent *bev, void *ctx);
static void async_handle_read_from_target(struct bufferevent *bev, void *ctx);
static void async_auth_func(struct bufferevent *bev, void *ctx);
static void close_on_finished_writecb(struct bufferevent *bev, void *ctx);
static void destroy_func(struct bufferevent *bev);
static void signal_func(evutil_socket_t sig_flag, short what, void *ctx);


static void
syntax(void)
{
  printf("Usage: esocks [options...]\n");
  printf("Options:\n");
  printf("  -a USERNAME:PASSWORD\n");
  printf("  -p port\n");
  printf("  -h host\n");
  printf("  -v enable verbose output\n");
  exit(EXIT_SUCCESS);
}

static void
signal_func(evutil_socket_t sig_flag, short what, void *ctx)
{
  struct event_base *base = ctx;
  struct timeval delay = {1, 0};
  int sec = 1;
  
  logger_err("Caught an interupt signal; exiting cleanly in %d second(s)", sec);
  event_base_loopexit(base, &delay);
}

static void
destroy_func(struct bufferevent *bev)
{
  /* let's destroy this buffer */
  logger_err("version is so wrong");
  logger_info("status=%d", status);
  bufferevent_trigger_event(bev, BEV_EVENT_ERROR, 0);
}

/* taken from libevent's sample le-proxy.c */
static void
close_on_finished_writecb(struct bufferevent *bev, void *ctx)
{
  struct evbuffer *evb = bufferevent_get_output(bev);

  if (evbuffer_get_length(evb) == 0) {
    logger_info("freed0");
    bufferevent_free(bev);
  }
}

static void
event_func(struct bufferevent *bev, short what, void *ctx)
{  

  struct bufferevent *associate = ctx;
      
  if (what & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {

    if (associate) {
      if (evbuffer_get_length(
			      bufferevent_get_output(associate))) {
	/* We still have to flush data from the other 
	 * side, but when it's done close the other 
	 * side. */
	bufferevent_setcb(associate, NULL,
			  close_on_finished_writecb, event_func, NULL);
	bufferevent_disable(associate, EV_READ);
      } else {
	/* We have nothing left to say to the other 
         * side; close it! */
	bufferevent_free(associate);
      }
    }
    bufferevent_free(bev);
    logger_debug(verbose, "freed");
  }
}

static void
socks_init_func(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src;
  ev_ssize_t evsize;
  size_t buf_size;

  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
    
  u8 reqbuf[buf_size];
  evsize = evbuffer_copyout(src, reqbuf, buf_size);

  /* its' important to send out thses two bytes */
  u8 payload[2] = {5, 0};

  if (auth) {
    payload[1] = 2;
  }
  
  if (reqbuf[0] == SOCKS_VERSION) {
    logger_debug(verbose, "getting a request");
    status = SINIT;
    if (bufferevent_write(bev, payload, 2)<0) {
      logger_err("socks_init_func.bufferevent_write");
      destroy_func(bev);
      return;
    }
    if (auth) {
      logger_debug(verbose, "callback to auth_func");
      bufferevent_setcb(bev, async_auth_func,
			NULL, event_func, associate);
      return;
    }
    evbuffer_drain(src, evsize);
    bufferevent_setcb(bev, async_read_func,
		      async_write_func, event_func, associate);
    bufferevent_enable(bev, EV_READ|EV_WRITE);    
    return;
  }
  /* This is not a right protocol; get this destroyed. */
  destroy_func(bev);
  logger_err("wrong protocol=%d", reqbuf[0]);
  return;
}

static void
async_auth_func(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src;
  u8 *buffer;
  size_t buf_size;
  u8 payload[2] = {5, 0};
  char *authbuf;
  char *user;
  char *passwd;
  int userlen, passwdlen, method, pad;
  size_t authlen;

  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
  buffer = calloc(buf_size, sizeof(ev_int8_t));  
  
  /* require clients to send username and password */
  payload[1] = SOCKSAUTHPASSWORD;
  
  if (evbuffer_copyout(src, buffer, buf_size)<0)
    logger_err("async_auth_func.evbuffer_copyout");

  /* check auth methods here.. */
  switch (buffer[1]) {
  case SOCKSAUTHPASSWORD:
    logger_info("auth userlenname/password");
    method = buffer[1];
    userlen = buffer[5];
    pad = 6;
    passwdlen = buffer[pad+userlen];
    break;
  case GSSAPI:
    logger_info("auth GSSAPI");
    method = buffer[1];
    userlen = buffer[4];
    pad = 5;
    passwdlen = buffer[pad+userlen];
    break;
  case IANASSIGNED:
  case 4:
  case 5:
  case 6:
  case 7:
    logger_info("auth IANA assigned");
    method = buffer[1];
    userlen = buffer[6];    
    pad = 7;
    passwdlen = buffer[pad+userlen];
    break;
  default:
    logger_err("auth method(%d) is not supported!", buffer[1]);
    destroy_func(bev);
    return;
  }
  
  logger_debug(verbose, "auth method=%d;userl=%d;passwdlen=%d", method, userlen, passwdlen);
  
  user = calloc(userlen, sizeof(char));
  passwd = calloc(passwdlen, sizeof(char));
  authbuf = calloc(userlen+passwdlen+1, sizeof(char));
  
  authlen = userlen + passwdlen
                               + 1  /* an extra for the colon */
                               + 1; /* an extra for the zero  */

  memcpy(user, buffer+pad, userlen);
  memcpy(passwd, buffer+pad+userlen+1, passwdlen);

  evutil_snprintf(authbuf, authlen, "%s:%s", user, passwd);  

  /* this is too rough authentication! 
     Should refactor.
  */
  if (strcmp(authbuf, auth) == 0) {
    logger_info("authenticated");    

    payload[1] = 0;
    /* send auth message */
    if (bufferevent_write(bev, payload, 2)<0) {
      logger_err("socks_init_func.bufferevent_write");
    }
  } else {
    status = SDESTROY;
  }
  
  if (status == SDESTROY) {
    logger_info("destroy");
    destroy_func(bev);
  } else {
    evbuffer_drain(src, buf_size);
    bufferevent_setcb(bev, async_read_func,
    		      async_write_func, event_func, associate);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
  }
}

static void
async_read_func(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src;  
  static struct addrspec *spec;
  u8 *buffer;
  size_t buf_size;
  u8 payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
  
  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
  buffer = calloc(buf_size, sizeof(ev_int8_t));
  
  if (evbuffer_copyout(src, buffer, buf_size)<0)
    logger_err("async_read_func.evbuffer_copyout");

  if (auth) {
    /* authentication code  */
    payload[1] = 2;
  }
  
  if (status == SINIT) {    

    /* parse socks header */
    switch (buffer[1]) {
    case CONNECT:
    case BIND:
      spec = handle_addrspec(buffer);
      if (!spec) {
	payload[1] = HOST_UNREACHABLE;
      }
      break;
    case UDPASSOC:
      logger_warn("udp associate(%d) is not supported", buffer[1]);
      payload[1] = NOT_SUPPORTED;
      status = SDESTROY;
      spec = NULL;
      break;
    default:
      logger_err("unknown command %d", buffer[1]);
      destroy_func(bev);
      return;
    }
    
    debug_addr(spec);
    
    if (!spec) {
      /* getaddrinfo failed? */
      logger_warn("spec cannot be NULL");
      if (bufferevent_write(bev, payload, 10)<0)
	logger_err("bufferevent_write");
      destroy_func(bev);      
      return;
      
    } else {
      bufferevent_enable(bev, EV_WRITE);
      status = SREAD;

     /* TODO: */
     /*    how about IPv6?? */
      if (spec->family == AF_INET)  {
	/* get this client ready to write */
	/* connects to a target and sets up next events */
	struct sockaddr_in target;
	target.sin_family = AF_INET;
	target.sin_addr.s_addr = (*spec).s_addr;
	target.sin_port = htons((*spec).port);

	if (bufferevent_socket_connect(associate,
				       (struct sockaddr*)&target, sizeof(target))<0){	
	  logger_err("failed to connect");
	  status = SDESTROY;	
	  payload[1] = HOST_UNREACHABLE;	
	  if (bufferevent_write(bev, payload, 10)<0) {
	    logger_err("bufferevent_write");
	  }
	  destroy_func(bev);
	  return;
	}
	evbuffer_drain(src, buf_size);
	logger_debug(verbose, "socket_connect and drain=%ld", buf_size);
	return;
      }
    }
  }
  
  if (status == SREAD) {
    /* make sure we already have a connection and pull the payload from a client */
    if (bufferevent_write(associate, buffer, buf_size)<0) {
      logger_err("async_read_func.bufferevent_write");
      status = SDESTROY;
    } else {
      logger_debug(verbose, "wrote to target=%ld bytes", buf_size);
      evbuffer_drain(src, buf_size);
      logger_debug(verbose, "drain=%ld", buf_size);    
      bufferevent_setcb(associate, async_handle_read_from_target,
			NULL, event_func, bev);
      bufferevent_enable(associate, EV_READ|EV_WRITE);
      return;
    }
  }  
  if (status == SDESTROY) {
    destroy_func(bev);    
    return;
  }  
}

static void
async_handle_read_from_target(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src, *dst;
  size_t buf_size;
  
  src = bufferevent_get_input(bev);  /* first pull payload from this client */
  buf_size  = evbuffer_get_length(src);

  if (!associate) {
    /* client left early?? */
    logger_err("asyn_handle_read_from_target: client left");
    destroy_func(bev);
    return;
  }
  
  dst = bufferevent_get_output(associate);
  evbuffer_add_buffer(dst, src);
  evbuffer_drain(src, buf_size);
}

static void
async_write_func(struct bufferevent *bev, void *ctx)
{
  u8 payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
  
  if (status == SINIT) {
    if (bufferevent_write(bev, payload, 10)<0) {
      logger_err("async_read_func._write set to SDESTROY");
      destroy_func(bev);
      return;
    }    
    logger_debug(verbose, "async_write_func: wrote");
    /* choke client */
    bufferevent_disable(bev, EV_WRITE);    
  }
}

static void
accept_func(struct evconnlistener *listener,
	    evutil_socket_t fd,
	    struct sockaddr *a, int slen, void *p)
{
  /* 
     Both src and dst will have a talk over a bufferevent.
  */
  struct bufferevent *src, *dst;
  src = bufferevent_socket_new(base, fd,
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  /* fd should be -1 here since we have no fd whatsoever */
  dst = bufferevent_socket_new(base, -1, 
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  bufferevent_setcb(src, socks_init_func, NULL, event_func, dst);
  bufferevent_enable(src, EV_READ|EV_WRITE);
}

int
main(int argc, char **argv)
{  
  struct options {
    const char *port;
    const char *host;
    const char *auth;
  };
  struct options o;
  char opt;  

  int socklen, port;  
  struct evconnlistener *listener;  
  static struct sockaddr_storage listen_on_addr;
  struct event *signal_event;
  
  if (argc < 2 || (strcmp(argv[1], "--help") == 0)) {
    syntax();
  }

  memset(&o, 0, sizeof(o));

  while ((opt = getopt(argc, argv, "h:p:a:vuqcs")) != -1) {
    switch (opt) {
    case 'v': ++verbose; break;
    case 'h': o.host = optarg; break;
    case 'p': o.port = optarg; break;
    case 'a': o.auth = optarg; break;
    default: fprintf(stderr, "Unknow option=%c\n", opt); break;      
    }
  }

  if (!o.host) {
    /* htonl(0x7f000001) == 127.0.0.1 */
    o.host = "0.0.0.0";
  }
  if (!o.port) {
    syntax();
  }
  if (!o.auth) {
    logger_warn("running without authentication...");    
  } else {
    auth = calloc(strlen(o.auth), sizeof(char));
    auth = o.auth;
  }

  /* allocate space for sockaddr_in */
  memset(&listen_on_addr, 0, sizeof(listen_on_addr));
  socklen = sizeof(listen_on_addr);
  
  if (evutil_parse_sockaddr_port(o.port, (struct sockaddr*)&listen_on_addr, &socklen)<0) {
    struct sockaddr_in *sin = (struct sockaddr_in*)&listen_on_addr;
    port = atoi(o.port);
    if (port < 1 || port > 65535)
      syntax();
    (*sin).sin_port = htons(port);
    if (evutil_inet_pton(AF_INET, o.host, &((*sin).sin_addr))<0)
      syntax();
    (*sin).sin_family = AF_INET;
    socklen = sizeof(struct sockaddr_in);
  }

  base = event_base_new();
  if (!base) {
    logger_errx(1, "event_base_new()");
  }

  listener = evconnlistener_new_bind(base, accept_func, NULL,
		     LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC,
				     -1, (struct sockaddr*)&listen_on_addr, socklen);
  if (!listener) {
    logger_err("evconnlistener_new_bind()");
    event_base_free(base);
    exit(EXIT_FAILURE);    
  }

  logger_info("Server is up and running %s:%s", o.host, o.port);
  logger_info("level=%d", verbose);

  signal_event = event_new(base, SIGINT,
			   EV_SIGNAL|EV_PERSIST, signal_func, (void*)base);
  
  if (!signal_event || event_add(signal_event, NULL)) {
    logger_errx(1, "Cannot add a signal_event");
  }
  
  event_base_dispatch(base);
  event_base_free(base);
  exit(EXIT_SUCCESS);
}
