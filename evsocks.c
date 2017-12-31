/* 
 * Use of this source code is governed by a
 * license that can be found in the LICENSE file.
 *
 * Simple proxy server with Libevent 
 * 
*/

#if defined(__APPLE__) && defined(__clang__)
#program clang diagnostic ignored "-Wdeprecated-declarations"
#endif

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

#include <event2/listener.h>

#include "internal.h"
#include "slog.h"

#define MAX_OUTPUT (512 * 1024)

const char *auth = NULL;

static struct event_base *base;

/* status holds a future event status */
static int status;
static void syntax(void);
static void eventcb(struct bufferevent *bev, short what, void *ctx);
static void acceptcb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *a, int slen, void *p);
static void socks_initcb(struct bufferevent *bev, void *ctx);
static void readcb(struct bufferevent *bev, void *ctx);
static void writecb(struct bufferevent *bev, void *ctx);
static void readcb_from_target(struct bufferevent *bev, void *ctx);
static void authorize_cb(struct bufferevent *bev, void *ctx);
static void close_on_finished_writecb(struct bufferevent *bev, void *ctx);
static void drained_writecb(struct bufferevent *bev, void *ctx);
static void after_connectcb(struct bufferevent *bev, void *ctx);
static void destroycb(struct bufferevent *bev);
static void signal_func(evutil_socket_t sig_flag, short what, void *ctx);


static void
syntax(void)
{
  printf("Usage: esocks [options...]\n");
  printf("Options:\n");
  printf("  -p port\n");
  printf("  -h host\n");
  printf("  -a USERNAME:PASSWORD\n");  
  printf("  -d debug\n");
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
destroycb(struct bufferevent *bev)
{
  /* let's destroy this buffer */
  status = 0;  
  bufferevent_free(bev);
}

/* taken from libevent's sample le-proxy.c */
static void
close_on_finished_writecb(struct bufferevent *bev, void *ctx)
{
  struct evbuffer *evb = bufferevent_get_output(bev);

  if (evbuffer_get_length(evb) == 0) {
    bufferevent_free(bev);
    logger_debug(verbose, "close_on_finished_writecb freed");    
  }
}

static void
eventcb(struct bufferevent *bev, short what, void *ctx)
{  

  struct bufferevent *partner = ctx;
  
  if (what & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
    
    if (what & BEV_EVENT_ERROR)
      logger_err("eventcb");
    
    if (partner) {

      /* Flush leftover */
      readcb_from_target(bev, partner);
      
      if (evbuffer_get_length(
			      bufferevent_get_output(partner))) {
	/* We still have to flush data from the other 
	 * side, but when it's done close the other 
	 * side. */
	bufferevent_setcb(partner, NULL,
			  close_on_finished_writecb, eventcb, NULL);
	bufferevent_disable(partner, EV_READ);
	bufferevent_enable(partner, EV_WRITE);	
      } else {
	/* We have nothing left to say to the other 
         * side; close it! */
	bufferevent_free(partner);
      }
    }
    logger_debug(verbose, "freed");
    bufferevent_free(bev);
  }
}

static void
socks_initcb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src = bufferevent_get_input(bev);
  size_t buf_size = evbuffer_get_length(src);
  ev_ssize_t evsize;

  /* its' important to send out thses two bytes */
  u8 payload[2] = {5, 0};
  u8 reqbuf[buf_size];

  evsize = evbuffer_copyout(src, reqbuf, buf_size);

  if (auth) {
    payload[1] = 2;
  }
  
  if (reqbuf[0] == SOCKS_VERSION) {
    logger_debug(verbose, "getting a request");
    status = SINIT;
    if (bufferevent_write(bev, payload, 2)<0) {
      logger_err("socks_initcb.bufferevent_write");
      destroycb(bev);
      return;
    }
    if (auth) {
      logger_debug(verbose, "callback to auth_func");
      bufferevent_setcb(bev, authorize_cb,
			NULL, eventcb, partner);
      return;
    }
    evbuffer_drain(src, evsize);
    bufferevent_setcb(bev, readcb,
		      writecb, eventcb, partner);
    bufferevent_enable(bev, EV_READ|EV_WRITE);    
  } else  {
    /* This is not a right protocol; get this destroyed. */
    destroycb(bev);
    logger_err("wrong protocol=%d", reqbuf[0]);
  }
}

static void
readcb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src, *dst;
  static struct addrspec *spec;
  u8 payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};  
  size_t buflen;

  src = bufferevent_get_input(bev);
  buflen = evbuffer_get_length(src);
  
  u8 buffer[buflen];
  
  evbuffer_copyout(src, buffer, buflen);

  if (auth) {
    /* authentication code  */
    payload[1] = 2;
  }

  /* Check if version is correct and status is equal to INIT */
  if (status == SINIT && buffer[0] == SOCKS_VERSION) {

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
      logger_warn("udp partner(%d) is not supported", buffer[1]);
      payload[1] = NOT_SUPPORTED;
      spec = NULL;
      break;
    default:
      logger_err("unknown command %d", buffer[1]);
      payload[1] = GENERAL_FAILURE;
      spec = NULL;      
    }
    
    debug_addr(spec);
    
    if (spec == NULL) {
      /* spec cannot be NULL */
      logger_info("destroy");
      if (bufferevent_write(bev, payload, 10)<0)
	logger_err("bufferevent_write");
      destroycb(bev);
      return;
      
    } else {
      
      bufferevent_enable(bev, EV_WRITE);      
     /* TODO: */
     /*    how about IPv6?? */
      if (spec->family == AF_INET)  {
	struct sockaddr_in target;
	target.sin_family = AF_INET;
	target.sin_addr.s_addr = spec->s_addr;
	target.sin_port = htons(spec->port);
	
	free(spec);
	
	if (bufferevent_socket_connect(partner,
				       (struct sockaddr*)&target, sizeof(target))<0){	
	  logger_err("failed to connect");
	  payload[1] = HOST_UNREACHABLE;	
	  if (bufferevent_write(bev, payload, 10)<0) {
	    logger_err("bufferevent_write");
	  }
	  destroycb(bev);
	  return;
	}
	
	evbuffer_drain(src, buflen);
	
	bufferevent_setcb(bev, after_connectcb, NULL, eventcb, partner);
	bufferevent_enable(bev, EV_WRITE|EV_READ);
      }
    }
  }
}

static void
after_connectcb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src;
  size_t buflen;

  src = bufferevent_get_input(bev);
  buflen = evbuffer_get_length(src); 
  
  u8 buffer[buflen];

  evbuffer_copyout(src, buffer, buflen);

  bufferevent_write(partner, buffer, buflen);

  /* Don't forget to drain buffer */
  evbuffer_drain(src, buflen);
  bufferevent_setcb(partner, readcb_from_target,
		    NULL, eventcb, bev);
  bufferevent_enable(partner, EV_READ|EV_WRITE);
}

static void
readcb_from_target(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src, *dst;
  size_t buflen;
  
  src = bufferevent_get_input(bev);
  buflen  = evbuffer_get_length(src);

  /* partner is a client. */
  /* bev is a target.   */
  if (!partner) {
    logger_debug(verbose, "readcb_from_target drain");
    evbuffer_drain(src, buflen);
    return;
  }

  /* Let's see payload for client. */
  dst = bufferevent_get_output(partner);

  /* Send data to the other side */
  if (evbuffer_add_buffer(dst, src)<0) {
    destroycb(partner);
  }

  if (evbuffer_get_length(dst) >= MAX_OUTPUT) {
    logger_debug(verbose, "exceeding MAX_OUTPUT %ld", evbuffer_get_length(dst));
    bufferevent_setcb(partner, NULL, drained_writecb, eventcb, bev);
    bufferevent_setwatermark(partner, EV_WRITE, MAX_OUTPUT/2,
 			     MAX_OUTPUT);
    bufferevent_disable(partner, EV_READ);
  }
}

static void
drained_writecb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  logger_debug(verbose, "EXCEEDING MAX_OUTPUT %ld",
	       evbuffer_get_length(bufferevent_get_output(bev)));  
  bufferevent_setcb(partner, readcb_from_target,
		    NULL, eventcb, bev);
  bufferevent_setwatermark(partner, EV_WRITE, 0, 0);
  bufferevent_enable(bev, EV_READ);
}

static void
writecb(struct bufferevent *bev, void *ctx)
{
  u8 payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
  
  if (status == SINIT) {
    if (bufferevent_write(bev, payload, 10)<0) {
      destroycb(bev);
      return;
    }    
    /* choke client */
    bufferevent_disable(bev, EV_WRITE);    
  }
}

static void
acceptcb(struct evconnlistener *listener,
	    evutil_socket_t fd,
	    struct sockaddr *a, int slen, void *p)
{
  struct bufferevent *src, *dst;
  src = bufferevent_socket_new(base, fd,
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  /* fd should be -1 here since we have no fd whatsoever */
  dst = bufferevent_socket_new(base, -1, 
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  bufferevent_setcb(src, socks_initcb, NULL, eventcb, dst);
  
  bufferevent_enable(src, EV_READ|EV_WRITE);
}

int
main(int argc, char **argv)
{  
  struct options {
    const char *port;
    const char *host;
    const char *auth;
    const char *nameserver;
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

  while ((opt = getopt(argc, argv, "h:p:adn:")) != -1) {
    switch (opt) {
    case 'd': ++verbose; break;
    case 'h': o.host = optarg; break;
    case 'p': o.port = optarg; break;
    case 'a': o.auth = optarg; break;
    case 'n': o.nameserver = optarg; break;
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
  if (o.auth) {
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
    sin->sin_port = htons(port);
    if (evutil_inet_pton(AF_INET, o.host, &(sin->sin_addr))<0)
      syntax();
    sin->sin_family = AF_INET;
    socklen = sizeof(struct sockaddr_in);
  }

  base = event_base_new();
  if (!base) {
    logger_errx(1, "event_base_new()");
  }

  listener = evconnlistener_new_bind(base, acceptcb, NULL,
		     LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC,
				     -1, (struct sockaddr*)&listen_on_addr, socklen);
  if (!listener) {
    logger_err("evconnlistener_new_bind()");
    event_base_free(base);
    exit(EXIT_FAILURE);    
  }

  logger_info("%s:%s", o.host, o.port);
  logger_info("level=%d", verbose);

  signal_event = event_new(base, SIGINT,
			   EV_SIGNAL|EV_PERSIST, signal_func, (void*)base);

  if (!signal_event || event_add(signal_event, NULL)) {
    logger_errx(1, "Cannot add a signal_event");
  }
  
  event_base_dispatch(base);
  event_base_free(base);
  exit(0);
}


static void
authorize_cb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src;
  u8 *buffer, payload[2] = {5, 0};
  char *authbuf, *user, *passwd;
  int userlen, passwdlen, method, pad;
  size_t buf_size, authlen;

  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
  buffer = calloc(buf_size, sizeof(u8));  
  
  /* require clients to send username and password */
  payload[1] = SOCKSAUTHPASSWORD;
  
  if (evbuffer_copyout(src, buffer, buf_size)<0)
    logger_err("authorize_cb.evbuffer_copyout");

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
    free(buffer);
    destroycb(bev);
    return;
  }
  
  logger_debug(verbose, "auth method=%d;userlen=%d;passwdlen=%d", method, userlen, passwdlen);
  
  user = calloc(userlen, sizeof(char));
  passwd = calloc(passwdlen, sizeof(char));
  authbuf = calloc(userlen+passwdlen+1, sizeof(char));
  
  authlen = userlen + passwdlen
                               + 1  /* an extra for the colon */
                               + 1; /* an extra for the zero  */

  memcpy(user, buffer+pad, userlen);
  memcpy(passwd, buffer+pad+userlen+1, passwdlen);
  
  free(buffer);
  
  evutil_snprintf(authbuf, authlen, "%s:%s", user, passwd);  

  /* this is too rough authentication! 
     Should refactor.
  */
  if (strcmp(authbuf, auth) == 0) {
    logger_info("authenticated");    
    payload[1] = SUCCESSED;
    /* send auth message */
    if (bufferevent_write(bev, payload, 2)<0) {
      logger_err("socks_initcb.bufferevent_write");
      free(user);
      free(passwd);
      free(authbuf);
      destroycb(bev);
      return;
    }
  } else {
    free(user);
    free(passwd);
    free(authbuf);
    destroycb(bev);
    return;
  }

  free(user);
  free(passwd);
  free(authbuf);  
  evbuffer_drain(src, buf_size);
  bufferevent_setcb(bev, readcb,
		    writecb, eventcb, partner);
  bufferevent_enable(bev, EV_READ|EV_WRITE);
}
