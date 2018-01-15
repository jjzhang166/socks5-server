/* 
 * Watch out - this is a work in progress!
 *
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

/* verbose output */
static int verbose_flag;

static struct event_base *base;

static struct evdns_base *evdns_base;

/* status holds a future event status */
static int status;
static void syntax(void);
static void eventcb(struct bufferevent *bev, short what, void *ctx);
static void acceptcb(struct evconnlistener *listener, evutil_socket_t fd,
		     struct sockaddr *a, int slen, void *p);
static void socks_initcb(struct bufferevent *bev, void *ctx);
static void local_writecb(struct bufferevent *bev, void *ctx);
static void readcb_from_target(struct bufferevent *bev, void *ctx);
static void close_on_finished_writecb(struct bufferevent *bev, void *ctx);
static void drained_writecb(struct bufferevent *bev, void *ctx);
static void local_readcb(struct bufferevent *bev, void *ctx);
static void remote_readcb(struct bufferevent *bev, void *ctx);
static void destroycb(struct bufferevent *bev);
static void signal_func(evutil_socket_t sig_flag, short what, void *ctx);


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
    logger_debug(DEBUG, "connected=%d", status);

  if (what & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
    
    if (what & BEV_EVENT_ERROR)
      logger_err("eventcb");
    
    if (partner) {

      /* Flush leftover */
      readcb_from_target(bev, ctx);
      
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

  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;

  int domlen, buflen;
  char *name;

  u8 abuf[128];
  u8 buf[buf_size];
  u8 v4[4];
  u8 v6[16];
  u8 portbuf[2];
  u16 port;
  
  int i;
  
  /* its' important to send out thses two bytes */  
  u8 payload[2] = {5, 0};

  evbuffer_copyout(src, buf, buf_size);
  
  evbuffer_drain(src, buf_size);
  
  /* TODO: */
  /*   Consider where and when data should be encrypted/decrypted */

  if (buf[0] == SOCKS_VERSION) {
    
    logger_debug(DEBUG, "connecting");
    
    status = SINIT;

    if (yes_this_is_local)
      {
    
	if (bufferevent_write(bev, payload, 2)<0) {
      
	  logger_err("socks_initcb.bufferevent_write");
      
	  destroycb(bev);
 
	  return;
	}
	
	bufferevent_setcb(bev, local_readcb, local_writecb,
			  eventcb, partner);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
	
      }
    if (!yes_this_is_local)
      {
	/* choke til we have a connection */
	bufferevent_disable(bev, EV_READ);

	switch(buf[3]) {
	  
	case IPV4:
	  
	  /* Extract 4 bytes address */	  
	  memcpy(v4, buf + 4, sizeof(v4));

	  if (evutil_inet_ntop(AF_INET, v4, abuf,
			       SOCKS_INET_ADDRSTRLEN) == NULL)
	    {
	      logger_err("invalid v4 address");
	      destroycb(bev);
	      return;
	    }

	  logger_debug(DEBUG, "seems legit v4=%s", abuf);
	  
	  if (evutil_inet_pton(AF_INET, abuf, &sin.sin_addr)<1)
	    {
	      logger_err("failed to resolve addr");
	      destroycb(bev);
	      return;
	    }
	  
	  /* And extract 2 bytes port as well */
	  memcpy(portbuf, buf + 4 + 4, 2);
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

	  /* wait for target's response and send it back
	   *to client 
	   */
	  bufferevent_setcb(bev, remote_readcb,
			    NULL, eventcb, partner);
	  bufferevent_enable(bev, EV_READ|EV_WRITE);

	  break;
	case IPV6:

	  memcpy(v6, buf + 4, sizeof(v6));

      /* Extract 16 bytes address */
      if (evutil_inet_pton(AF_INET6, v6, &sin6.sin6_addr)<1)
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

	/* wait for target's response and send it back
	 *to client 
	 */
	bufferevent_setcb(bev, remote_readcb,
			  NULL, eventcb, partner);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
	
	  break;	  
	case DOMAINN:

	  domlen = buf[4];
	  
	  buflen = domlen + 5;

	  name = (char*)malloc(domlen+1);
   
	  memcpy(name, buf + 5, domlen);

	  logger_debug(DEBUG, "domain=>%s", name);
    
	  free(name);
	  
	  /* We should take domains more carefully than ipv4 and ipv6 */
	  
	  break;

	default:
	  logger_err("unkown atype=%d", buf[3]);
	  destroycb(bev);
	  return;
	}

      }
    
  } else {
    /* Seems a wrong protocol; get this destroyed. */
    logger_err("wrong protocol=%d", buf[0]);
    destroycb(bev);
  }
}


static void
remote_readcb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src = bufferevent_get_input(bev);
  
  size_t buf_size = evbuffer_get_length(src);
  
  u8 buf[buf_size];

  evbuffer_copyout(src, buf, buf_size);  
  evbuffer_drain(src, buf_size);
  
  logger_debug(DEBUG, "payload to targ=%ld", buf_size);
  
  bufferevent_write(partner, buf, buf_size);  
  
  bufferevent_setcb(partner, readcb_from_target,
		    NULL, eventcb, bev);
  bufferevent_enable(partner, EV_READ|EV_WRITE);
}


typedef enum {
  CONNECT = 1,
  BIND,
  UDPASSOC
} socks_cmd_e;


static void
local_readcb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src;
  size_t buf_size;
  
  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
  
  u8 buf[buf_size];

  evbuffer_copyout(src, buf, buf_size);  
  
  socks_cmd_e cmd = buf[1];

  /* Check if version is correct and status is equal to INIT */
  if (status == SINIT && buf[0] == SOCKS_VERSION)
    {
      /* parse socks header */
      switch (cmd) {
      case CONNECT:
      case BIND:
	break;
      case UDPASSOC:
	logger_warn("udp associate");
	break;
      default:
	logger_warn("unkonw cmd=%d", buf[1]);
	destroycb(bev);
	return;
      }  
    }

  evbuffer_drain(src, buf_size);
  
  bufferevent_write(partner, buf, buf_size);

  /* let bev write some data since it gets choked.. */
  bufferevent_enable(bev, EV_WRITE);

  /* set callbacks and wait for server response */  
  bufferevent_setcb(partner, readcb_from_target, NULL, eventcb, bev);
  bufferevent_enable(partner, EV_WRITE|EV_READ);
}


static void
local_writecb(struct bufferevent *bev, void *ctx)
{  
  struct bufferevent *partner = ctx;
  struct evbuffer *src = bufferevent_get_input(bev);
  size_t buf_size = evbuffer_get_length(src);  
  u8 payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
  u8 buf[buf_size];

  evbuffer_copyout(src, buf, buf_size);
  evbuffer_drain(src, buf_size);
  
  if (status == SINIT) {
    bufferevent_write(bev, payload, 10);
    bufferevent_disable(bev, EV_WRITE);
  }

  /* change status SINIT to whatever status */
  status = SWAIT;
}


static void
readcb_from_target(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src, *dst;
  size_t buf_size;
  
  src = bufferevent_get_input(bev);

  buf_size  = evbuffer_get_length(src);
  u8 buf[buf_size];
  
  if (!partner) {

    logger_debug(DEBUG, "readcb_from_target drain");
    
    evbuffer_drain(src, buf_size);

    return;
  }

  evbuffer_copyout(src, buf, buf_size);
  
  bufferevent_write(partner, buf, buf_size);
  
  evbuffer_drain(src, buf_size);
  
  logger_info("drained=%ld", buf_size);

  /* forward  */
  dst = bufferevent_get_output(partner);
  
  if (evbuffer_get_length(dst) >= MAX_OUTPUT) {
    
    logger_debug(DEBUG, "exceeding MAX_OUTPUT %ld",
		 evbuffer_get_length(dst));
    
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

static void
syntax(void)
{  
  printf("Usage: esocks [options]\n");
  printf("Options:\n");
  printf(" -s --server_addr  ADDRESS\n");
  printf(" -p --server_port  PORT\n");  
  printf(" -u --local_addr   ADDRESS\n");
  printf(" -j --local_port   PORT\n");
  printf(" -k --password     PASSWORD\n");
  // printf(" -w --worker       WORKERS");
  // printf(" -b --backend      BACKEND");
  printf("\n");
  exit(1);
}


static void
signal_func(evutil_socket_t sig_flag, short what, void *ctx)
{
  struct event_base *base = ctx;
  struct timeval delay = {1, 0};
  int sec = 1;
  
  logger_info(
       "Caught an interupt signal; exiting cleanly in %d second(s)", sec);
  event_base_loopexit(base, &delay);
}


int
main(int c, char **v)
{
  struct event_config *config;
  const char *levent_ver = event_get_version();  

  int cc;
  struct options {
    const char *server_addr;    
    const char *server_port;    
    const char  *local_addr;
    const char  *local_port;        
    const char    *password;
  };

  struct options o;
  char opt;  

  memset(&o, 0, sizeof(o));
  
  while (1)
    {
      static struct option lp[] =
	{
	  {"verbose_flag", no_argument, &verbose_flag, 1},
	  {"local",        no_argument, &yes_this_is_local, 1},
	  {"server_addr", required_argument, 0, 's'},
	  {"server_port", required_argument, 0, 'p'},
	  {"local_addr",  required_argument, 0, 'u'},
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
	case 's': o.server_addr = optarg; break;
	case 'p': o.server_port = optarg; break;
	case 'u': o.local_addr  = optarg; break;
	case 'j': o.local_port  = optarg; break;
	case 'k': o.password    = optarg; break;
	case 'l':      yes_this_is_local; break;
	case 'v':         verbose_flag++; break;
	case '?':               syntax(); break;
	}
    }

  if (yes_this_is_local) {
    if (!(o.local_port && o.local_addr && o.password &&
	  o.server_addr && o.server_port))
      syntax();
  } else { /* forward server */
    if (!(o.server_addr && o.server_port && o.password))
      syntax();
  }

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
      if (evutil_parse_sockaddr_port(o.local_port,
			   (struct sockaddr*)&listen_on_addr, &socklen)<0)
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
      
      /* Prep forward server */
      if (evutil_parse_sockaddr_port(o.server_port,
			      (struct sockaddr*)&forward_addr, &socklen)<0)
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
      
      listener = evconnlistener_new_bind(base, acceptcb,
	    (struct sockaddr*)&forward_addr,
	    LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC,
		           -1, (struct sockaddr*)&listen_on_addr, socklen);
      logger_info("server is up and running %s:%s connecting %s:%s",
	  o.local_addr, o.local_port, o.server_addr, o.server_port);
    }
  else
    {
      /* Running as forward server */
      if (evutil_parse_sockaddr_port(o.server_port,
		     (struct sockaddr*)&listen_on_addr, &socklen)<0)
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
  
  logger_info("`%s` mode",
	      yes_this_is_local ? "local" : "remote");
  logger_info("Libevent version: %s", levent_ver);
  
  event_base_dispatch(base);
  event_base_free(base);
  
  return 0;
}
