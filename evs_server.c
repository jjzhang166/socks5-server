/* 
 * server.c
 *
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

#include <sys/stat.h>
#include <getopt.h>
#include <signal.h>


#include <event2/listener.h>

#include "evs_internal.h"
#include "evs_log.h"

#define MAX_OUTPUT (512 * 1024)

/* local flag */
static int yes_this_is_local;

const char *auth = NULL;

/* verbose for verbose log output */
static int verbose_flag;

static struct event_base *base;

static struct evdns_base *evdns_base;

/* status holds a future event status */
static int status;
static void syntax(void);
static void eventcb(struct bufferevent *bev, short what, void *ctx);
static void acceptcb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *a, int slen, void *p);
static void socks_initcb(struct bufferevent *bev, void *ctx);
static void readcb(struct bufferevent *bev, void *ctx);
static void local_writecb(struct bufferevent *bev, void *ctx);
static void readcb_from_target(struct bufferevent *bev, void *ctx);
static void authorize_cb(struct bufferevent *bev, void *ctx);
static void close_on_finished_writecb(struct bufferevent *bev, void *ctx);
static void drained_writecb(struct bufferevent *bev, void *ctx);
static void local_readcb(struct bufferevent *bev, void *ctx);
static void local_readcb_(struct bufferevent *bev, void *ctx);
static void remote_writecb(struct bufferevent *bev, void *ctx);
static void remote_readcb(struct bufferevent *bev, void *ctx);
static void destroycb(struct bufferevent *bev);
static void signal_func(evutil_socket_t sig_flag, short what, void *ctx);

static void
syntax(void)
{
  printf("Usage: esocks [options]\n");
  printf("Options:\n");
  printf(" -g --server_addr\n");
  printf(" -h --server_port\n");  
  printf(" -i --local_addr\n");
  printf(" -j --local_port\n");
  printf(" -k --password\n");
  printf(" -v --verbose\n");
  printf("\n");
  exit(1);
}

static void
signal_func(evutil_socket_t sig_flag, short what, void *ctx)
{
  struct event_base *base = ctx;
  struct timeval delay = {1, 0};
  int sec = 1;
  
  logger_info("Caught an interupt signal; exiting cleanly in %d second(s)", sec);
  event_base_loopexit(base, &delay);
}

static void
destroycb(struct bufferevent *bev)
{
  status = 0;
  
  logger_info("destroyed");
 
  /* Unset all callbacks */
  bufferevent_free(bev);
  
}

/* taken from libevent's sample le-proxy.c */
static void
close_on_finished_writecb(struct bufferevent *bev, void *ctx)
{
  struct evbuffer *evb = bufferevent_get_output(bev);

  if (evbuffer_get_length(evb) == 0) {
    
    bufferevent_free(bev);
    
    logger_debug(DEBUG, "close_on_finished_writecb freed");
    
  }
}

static void
eventcb(struct bufferevent *bev, short what, void *ctx)
{  

  struct bufferevent *partner = ctx;

  if (what & (BEV_EVENT_CONNECTED))
    logger_debug(DEBUG, "connected");

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
      } else
	/* We have nothing left to say to the other 
         * side; close it! */
	bufferevent_free(partner);
    }
    
    logger_debug(DEBUG, "freed");
    bufferevent_free(bev);
    
  }
}

static void
socks_initcb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src = bufferevent_get_input(bev);
  size_t buf_size = evbuffer_get_length(src);
  // ev_ssize_t evsize;
  int i;
  /* its' important to send out thses two bytes */  
  u8 payload[2] = {5, 0};
  
  u8 version[0];  
  /* read minimum buffer to get version */
  evbuffer_copyout(src, version, 1);

  /* TODO: */
  /*   Consider where and when data should be encrypted/decrypted */

  if (version[0] == SOCKS_VERSION) {    
    logger_debug(DEBUG, "connecting");
    
    status = SINIT;
    
    if (bufferevent_write(bev, payload, 2)<0) {
      
      logger_err("socks_initcb.bufferevent_write");
      
      destroycb(bev);
 
      return;
    }

    if (yes_this_is_local)
      {	
	/* Cut the socks header that is usually 5 0 0 1... */
	evbuffer_drain(src, buf_size);
	
	// bufferevent_setcb(bev, readcb, writecb, eventcb, partner);
	bufferevent_setcb(bev, local_readcb_, local_writecb, eventcb, partner);
	bufferevent_enable(bev, EV_READ|EV_WRITE);	
	logger_debug(DEBUG, "bufsize=%ld", buf_size);

      }
    else
      { /* Handling events in remote server */
	evbuffer_drain(src, 3);
	bufferevent_setcb(bev, remote_readcb,
			  remote_writecb, eventcb, partner);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
      }
    
  } else {
    /* This is not a right protocol; get this destroyed. */
    logger_err("wrong protocol=%d", version[0]);
    destroycb(bev);
  }
}


static void
local_readcb_(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src;
  
  u8 payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};

  size_t buflen;

  src = bufferevent_get_input(bev);
  buflen = evbuffer_get_length(src);
  
  u8 buffer[buflen];
  
  evbuffer_copyout(src, buffer, buflen);
  
  /* Check if version is correct and status is equal to INIT */
  if (status == SINIT && buffer[0] == SOCKS_VERSION)
    {
      switch(buffer[1]) {
	  /* parse socks header */
	  switch (buffer[1]) {
	  case CONNECT:
	  case BIND:	    
	    bufferevent_write(partner, buffer, buflen);
	    break;
	  case UDPASSOC:
	    logger_warn("udp associate");
	    break;
	  default:
	    logger_warn("unkonw cmd=%d", buffer[1]);
	    destroycb(bev);
	    return;
	  }
	}
    }
  bufferevent_write(bev, payload, 10);
  logger_info("send");
}


static void
local_writecb(struct bufferevent *bev, void *ctx)
{
  u8 payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
  
  if (status == SINIT) {
	
    if (bufferevent_write(bev, payload, 10)<0) {
	  
      destroycb(bev);
      
      return;
    }
    
    /* choke client */
    // bufferevent_disable(bev, EV_WRITE);
	
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
  
  /* Check if version is correct and status is equal to INIT */
  if (status == SINIT && buffer[0] == SOCKS_VERSION)
    {
      /* parse socks header */
      switch (buffer[1]) {
      case CONNECT:
      case BIND:
	spec = handle_addrspec(buffer);
	if (spec == NULL) {
	  logger_debug(DEBUG, "BIND and CONNECT and give us back NULL spec");
	  payload[1] = HOST_UNREACHABLE;
	}
	break;
      case UDPASSOC:
	logger_warn("protocol(%d) is not supported", buffer[1]);
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
	logger_info("spec is null and destroy");
	if (bufferevent_write(bev, payload, 10)<0)
	  logger_err("bufferevent_write");
	destroycb(bev);
	return;
      
      } else {
	/* Await partner's event and go to local_readcb*/ 
	bufferevent_setcb(bev, local_readcb, NULL, eventcb, partner);
	bufferevent_enable(bev, EV_WRITE|EV_READ);
      }
    }
}

static void
remote_readcb(struct bufferevent *bev, void *ctx)
{
  logger_errx(1, "called remote_readcb");
}

static void
remote_writecb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src = bufferevent_get_input(bev);
  
  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;
  
  size_t buf_size = evbuffer_get_length(src);
  int res = 0;
  int i;
  int domlen, buflen;
  char buf[128];
  char v4[4];
  char v6[16];  
  char *name;
  u8 reqbuf[buf_size];
  u8 portbuf[2];
  u16 port;
    
  evbuffer_copyout(src, reqbuf, buf_size);

  if (status == SCONNECTED) {
    logger_info("payload=%ld", buf_size);
    logger_errx(0, "already made a connection!");
  };
  
  switch(reqbuf[0]) {

  case IPV4:

    evbuffer_drain(src, 1+4+2);
    
    memcpy(v4, reqbuf+1, sizeof(v4));   

    if (evutil_inet_ntop(AF_INET, v4, buf, SOCKS_INET_ADDRSTRLEN) == NULL)
      {
	logger_err("invalid v4 address");
	destroycb(bev);
	return;
      }
    
    logger_debug(DEBUG, "seems legit v4=%s", buf);
    
    /* Extract 4 bytes address */
    if (evutil_inet_pton(AF_INET, buf, &sin.sin_addr)<1)
      {
	logger_err("failed to resolve addr");
	destroycb(bev);
	return;
      }
    
    /* And extract 2 bytes port as well */
    memcpy(portbuf, reqbuf+1+4, 2);
    port = portbuf[0]<<8 | portbuf[1];

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
  
    if (bufferevent_socket_connect(partner,
		   (struct sockaddr*)&sin, sizeof(sin)) != 0)
      {
	logger_err("v4: failed to connect");
	destroycb(bev);
	return;	
      }
  
    status = SCONNECTED;
  
    break;
  case IPV6:

    evbuffer_drain(src, 1+16+2);

    memcpy(v6, reqbuf+1, sizeof(v6));

    if (evutil_inet_ntop(AF_INET6, v6, buf, SOCKS_INET6_ADDRSTRLEN) == NULL)
      {
	logger_err( "invalid v6 address");
	destroycb(bev);
	return;
      }

    logger_debug(DEBUG, "seems legit v6=%s", buf);
    
    /* Extract 16 bytes address */
    if (evutil_inet_pton(AF_INET6, buf, &sin6.sin6_addr)<1)
      {
	logger_err("v6: failed to resolve addr");
	destroycb(bev);
	return;
      }
  
    if (bufferevent_socket_connect(partner,
			   (struct sockaddr*)&sin6, sizeof(sin6)) != 0)
      {
	logger_err("failed to connect");
	destroycb(bev);
	return;	
      }

    /* And extract 2 bytes port as well */
    memcpy(portbuf, reqbuf+1+16, 2);
    port = portbuf[0]<<8 | portbuf[1];

    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(port);
    
    status = SCONNECTED;
    
    break;
  // case _DOMAINNAME:
  // 
  //   domlen = reqbuf[4];
  //   
  //   buflen = domlen + 5;
  // 
  //   name = (char*)malloc(domlen+1);
  // 
  //   memcpy(name, reqbuf + 5, domlen);
  //   
  //   if (resolve_host(name, domlen, spec)<0)
  //     {
  // 	logger_err("failed to resolve addr");
  // 	destroycb(bev);
  // 	return;
  //     }
  // 
  //   free(name);
  //   
  //   logger_debug(DEBUG, "domain=>%s", spec->domain);
  //   break;
  default:
    
    logger_debug(DEBUG, "unknown code(%d)", reqbuf[3]);
    destroycb(bev);
    return;
  }
    
  bufferevent_setcb(partner, readcb_from_target,
		    remote_writecb, eventcb, bev);
  bufferevent_enable(partner, EV_READ|EV_WRITE);
}

static void
local_readcb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src;
  size_t buflen;

  src = bufferevent_get_input(bev);
  buflen = evbuffer_get_length(src); 
  
  u8 buffer[buflen];

  evbuffer_copyout(src, buffer, buflen);
  
  logger_debug(DEBUG, "sending %ld bytes to server ...", buflen);
  
  bufferevent_write(partner, buffer, buflen);

  /* Don't forget to drain buffer */
  evbuffer_drain(src, buflen);

  // bufferevent_setcb(partner, readcb_from_target,
  // 		    remote_writecb, eventcb, bev);
  // bufferevent_enable(partner, EV_READ|EV_WRITE);
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
    
    logger_debug(DEBUG, "readcb_from_target drain");
    
    evbuffer_drain(src, buflen);
    
    return;
  }

  /* Let's see payload for client. */
  dst = bufferevent_get_output(partner);

  /* Send data to the other side */
  evbuffer_add_buffer(dst, src);

  if (evbuffer_get_length(dst) >= MAX_OUTPUT) {
    
    logger_debug(DEBUG, "exceeding MAX_OUTPUT %ld", evbuffer_get_length(dst));
    
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
  
  logger_debug(DEBUG, "EXCEEDING MAX_OUTPUT %ld",
	       evbuffer_get_length(bufferevent_get_output(bev)));
  
  bufferevent_setcb(partner, readcb_from_target,
		    NULL, eventcb, bev);
  
  bufferevent_setwatermark(partner, EV_WRITE, 0, 0);
  bufferevent_enable(bev, EV_READ);
}

static void
acceptcb(struct evconnlistener *listener,
	 evutil_socket_t fd,
	 struct sockaddr *a, int slen, void *p)
{
  struct sockaddr *sa = p;
  struct bufferevent *bev, *partner;

  /* BEV_OPT_CLOSE_ON_FREE 
   *  close the underlying socket, free an underlying bufferevent
   * BEV_OPT_DEFER_CALLBACKS 
   *  Deffer callbacks
   */
  bev = bufferevent_socket_new(base, fd,
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  if (yes_this_is_local)
    {
      partner = bufferevent_socket_new(base, -1, 
	       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
      if (bufferevent_socket_connect(partner,
				     sa, sizeof(struct sockaddr_in))<0)
	{
	  logger_err("bufferevent_socket_connect");
	}
    }
  else 
    /* fd should be -1 here since we have no fd whatsoever */    
    partner = bufferevent_socket_new(base, -1, 
		     BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  assert(bev && partner);

  bufferevent_setcb(bev, socks_initcb, NULL, eventcb, partner);
  bufferevent_enable(bev, EV_READ|EV_WRITE);
}

int
main(int c, char **v)
{

  int cc;
  struct options {
    const char *server_addr;    
    const char *server_port;    
    const char  *local_addr;
    const char  *local_port;        
    const char    *password;
  };

  const char *servers[128];
  struct options o;
  char opt;  
  
  memset(&o, 0, sizeof(o));
  
  while (1)
    {
      static struct option lp[] =
	{
	  {"verbose_flag", no_argument, &verbose_flag, 1},
	  {"local",        no_argument, &yes_this_is_local, 1},
	  {"server_addr", required_argument, 0, 'g'},
	  {"server_port", required_argument, 0, 'h'},
	  {"local_addr",  required_argument, 0, 'i'},
	  {"local_port",  required_argument, 0, 'j'},
	  {"password",    required_argument, 0, 'k'}
	};

      int opt_index = 0;
      
      cc = getopt_long(c, v, "vlg:h:i:j:k:", lp, &opt_index);
      
      if (cc == -1)
	break;

      switch(cc)
	{
	case 0:
	  if (lp[opt_index].flag != 0) break;
	case 'g': o.server_addr = optarg; break;
	case 'h': o.server_port = optarg; break;
	case 'i': o.local_addr  = optarg; break;
	case 'j': o.local_port  = optarg; break;
	case 'k': o.password    = optarg; break;
	case 'l':              yes_this_is_local; break;
	case 'v':         verbose_flag++; break;
	case '?':               syntax(); break;
	default:                abort();
	}
    }
  
  if (!(o.server_addr||o.server_port||o.local_addr||o.local_port||o.password))
    syntax();

  int              fsocklen, socklen, port, mode;
  struct evconnlistener               *listener;  
  static struct sockaddr_storage listen_on_addr;
  static struct sockaddr_storage   forward_addr;  
  struct event                    *signal_event;
  
  memset(&listen_on_addr, 0, sizeof(listen_on_addr));
  socklen = sizeof(listen_on_addr);

  memset(&forward_addr, 0, sizeof(forward_addr));
  fsocklen = sizeof(forward_addr);
  
  base = event_base_new();
  
  if (yes_this_is_local)
    /* A server is requested running in local mode
       Server should connect to a remote server
    */
    {

      if (evutil_parse_sockaddr_port(o.local_port, (struct sockaddr*)&listen_on_addr, &socklen)<0)
	{
	  struct sockaddr_in *sin = (struct sockaddr_in*)&listen_on_addr;
	  port = atoi(o.local_port);
	  if (port < 1 || port > 65535)
	    syntax();
	  sin->sin_port = htons(port);
	  if (evutil_inet_pton(AF_INET, o.local_addr, &sin->sin_addr)<0)
	    syntax();
	  sin->sin_family = AF_INET; /* TODO IPv6 */
	  socklen = sizeof(struct sockaddr_in);
	}
      
      /* prep forward server */
      if (evutil_parse_sockaddr_port(o.server_port, (struct sockaddr*)&forward_addr, &socklen)<0)
	{
	  struct sockaddr_in *fsin = (struct sockaddr_in*)&forward_addr;
	  port = atoi(o.server_port);
	  if (port < 1 || port > 65535)
	    syntax();
	  fsin->sin_port = htons(port);
	  if (evutil_inet_pton(AF_INET, o.server_addr, &fsin->sin_addr)<0)
	    syntax();
	  fsin->sin_family = AF_INET; /* TODO IPv6 */
	  fsocklen = sizeof(struct sockaddr_in);
	}
      
      listener = evconnlistener_new_bind(base, acceptcb, (struct sockaddr*)&forward_addr,
					 LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC,
					 -1, (struct sockaddr*)&listen_on_addr, socklen);
      logger_info("server is up and running %s:%s connecting %s:%s",
		  o.local_addr, o.local_port, o.server_addr, o.server_port);      
    }
  else
    {
      /* Running as forward server */
      if (evutil_parse_sockaddr_port(o.server_port, (struct sockaddr*)&listen_on_addr, &socklen)<0)
	{
	  struct sockaddr_in *sin = (struct sockaddr_in*)&listen_on_addr;
	  port = atoi(o.server_port);
	  if (port < 1 || port > 65535)
	    syntax();
	  sin->sin_port = htons(port);
	  if (evutil_inet_pton(AF_INET, o.server_addr, &sin->sin_addr)<0)
	    syntax();
	  sin->sin_family = AF_INET; /* TODO IPv6 */
	  socklen = sizeof(struct sockaddr_in);
	}
      
      /* Ready for forward connections from clients */
      listener = evconnlistener_new_bind(base, acceptcb, NULL,
					 LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC,
					 -1, (struct sockaddr*)&listen_on_addr, socklen);
      logger_info("server is up and running %s:%s connecting %s:%s",
		  o.server_addr, o.server_port, o.local_addr, o.local_port);
    }

  if (!listener) {
    logger_err("bind");
    event_base_free(base);
    return 1;
  }

  if (DEBUG == 1) logger_debug(DEBUG, "DEBUG MODE");
  
  signal_event = event_new(base, SIGINT,
			   EV_SIGNAL|EV_PERSIST, signal_func, (void*)base);
  
  if (!signal_event || event_add(signal_event, NULL))
    logger_errx(1, "Cannot add a signal_event");
  
  logger_info("this is `%s` mode", yes_this_is_local ? "local" : "non-local");
  
  event_base_dispatch(base);
  
  event_base_free(base);
  
  return 0;
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
  
  logger_debug(DEBUG, "auth method=%d;userlen=%d;passwdlen=%d", method, userlen, passwdlen);
  
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
      logger_warn("login failed");
      destroycb(bev);
      return;
    }    
  } else {
    logger_warn("login failed");
    destroycb(bev);
    return;
  }
  free(user);
  free(passwd);
  free(authbuf);  
  evbuffer_drain(src, buf_size);
  bufferevent_setcb(bev, readcb,
		    local_writecb, eventcb, partner);
  bufferevent_enable(bev, EV_READ|EV_WRITE);  
}
