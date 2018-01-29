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

#ifdef FAST_OPEN /* Experimental, only for Linux */
#include <netinet/tcp.h>
#endif

#include <sys/stat.h>
#include <getopt.h>
#include <signal.h>

#include <event2/listener.h>

#include "evs_internal.h"
#include "evs_log.h"
#include "evs_helper.h"


static int status;
static int yes_this_is_local;
static int verbose_flag;
static struct event_base *base;
static struct evdns_base *evdns_base;


static void syntax(void);
static void eventcb(struct bufferevent *bev, short what, void *ctx);
static void acceptcb(struct evconnlistener *listener, evutil_socket_t fd,
		     struct sockaddr *a, int slen, void *p);
static void socks_initcb(struct bufferevent *bev, void *ctx);
static void readcb_from_target(struct bufferevent *bev, void *ctx);
static void close_on_finished_writecb(struct bufferevent *bev, void *ctx);
static void drained_writecb(struct bufferevent *bev, void *ctx);
static void local_readcb(struct bufferevent *bev, void *ctx);
static void local_writecb(struct bufferevent *bev, void *ctx);
static void remote_readcb(struct bufferevent *bev, void *ctx);
static void destroycb(struct bufferevent *bev);
static void signal_func(evutil_socket_t sig_flag, short what, void *ctx);
static void resolvecb(socks_name_t *);
static void app_fatal_cb(int err);
static void internal_logcb(int sev, const char *msg);


static void
internal_logcb(int sev, const char *msg)
{
  log_debug(DEBUG, "levent=%d; internal=%s", sev, msg);
}


static void
app_fatal_cb(int err)
{
  log_err("fatal %d", err);
  exit(1);
}

static void
destroycb(struct bufferevent *bev)
{
  status = 0;
  
  log_info("destroyed");

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
    
    log_debug(DEBUG, "close_on_finished_writecb freed");   
  }
}


static void
eventcb(struct bufferevent *bev, short what, void *ctx)
{  

  struct bufferevent *partner = ctx;

  // if (what & (BEV_EVENT_CONNECTED))
  //   log_debug(DEBUG, "connected=%d", status);

  if (what & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
    
    if (what & BEV_EVENT_ERROR)
      log_err("eventcb");
    
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
	bufferevent_disable(partner, EV_READ);
      } else
	/* We have nothing left to say to the other 
         * side; close it! */
	bufferevent_free(partner);
    }
    log_debug(DEBUG, "freed");
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
  struct sockaddr_storage storage;
  /* name lookup stuff */
  socks_name_t n;
  int domlen, buflen;
  char name;  
  
  u8 abuf[128];
  u8 buf[buf_size];
  u8 v4[4];
  u8 v6[16];
  u8 portbuf[2];
  u16 port;

  /* payload for ok message to clients */
  u8 payload[2] = {5, 0};

  evbuffer_copyout(src, buf, buf_size);

  /* TODO: */
  /*   Consider where and when data should be encrypted/decrypted */

  if (buf[0] == SOCKS_VERSION) {

    log_info("connect");
    
    status = SINIT;
    
    if (yes_this_is_local && status == SINIT)
      {
	
	log_debug(DEBUG, "local connect");
        evbuffer_drain(src, buf_size);
	
	if (bufferevent_write(bev, payload, 2)<0) {
      
	  log_err("bufferevent_write");      
	  destroycb(bev);

	  return;
	}

	bufferevent_setcb(bev, local_readcb, local_writecb,
			  eventcb, partner);
	bufferevent_enable(bev, EV_READ|EV_WRITE);	
      }

    /* Basically what we do here is resolve hosts and wait 
     * til we have a connection.
     */
    if (!yes_this_is_local && status == SINIT)
      {
	switch(buf[3]) {

	case IPV4:
	  
	  evbuffer_drain(src, buf_size);
	  
	  /* Extract 4 bytes address */	  
	  memcpy(v4, buf + 4, sizeof(v4));

	  if (evutil_inet_ntop(AF_INET, v4, abuf,
	   		       SOCKS_INET_ADDRSTRLEN) == NULL)
	    {
	      log_err("invalid v4 address");
	      destroycb(bev);
	      return;
	    }
	  
	  if (evutil_inet_pton(AF_INET, abuf, &sin.sin_addr) < 1)
	    {
	      log_err("failed to resolve addr");
	      destroycb(bev);
	      return;
	    }
	  
	  /* And extract 2 bytes port as well */
	  memcpy(portbuf, buf + 4 + 4, 2);
	  port = portbuf[0]<<8 | portbuf[1];

	  sin.sin_family = AF_INET;
	  sin.sin_port = htons(port);
	  
	  /* connect immediately if address is raw and legit */
	  if (bufferevent_socket_connect(partner,
				    (struct sockaddr*)&sin, sizeof(sin)) != 0)
	    {
	      log_err("connect: failed to connect");
	      destroycb(bev);
	      return;	      
	    }
	  log_debug(DEBUG, "v4 connect immediate");
	  status = SCONNECTED;
	  
	  break;
	case IPV6:
	  
	  evbuffer_drain(src, buf_size);
	  
	  if (evutil_inet_ntop(AF_INET6, buf + 4, abuf,
	   		       SOCKS_INET6_ADDRSTRLEN) == NULL)
	    {
	      log_err("invalid v6 address");
	      destroycb(bev);
	      return;
	    }

	  /* Extract 16 bytes address */
	  if (evutil_inet_pton(AF_INET6, abuf, &sin6.sin6_addr) < 1)
	    {
	      log_err("v6: failed to resolve addr");
	      destroycb(bev);
	      return;
	    }

	  /* And extract 2 bytes port as well */
	  memcpy(portbuf, buf + 4 + 16, 2);
	  port = portbuf[0]<<8 | portbuf[1];

	  sin6.sin6_family = AF_INET6;
	  sin6.sin6_port = htons(port);

	  /* connect immediately if address is raw and legit */
	  if (bufferevent_socket_connect(partner,
				 (struct sockaddr*)&sin6, sizeof(sin6)) != 0)
	    {
	      log_err("connect: failed to connect");
	      destroycb(bev);
	      return;
	    }

	  log_debug(DEBUG, "connect immediate to %s", abuf);    
	  status = SCONNECTED;
	  
	  break;	  
	case DOMAINN:
	  
	  domlen = (size_t) buf[4];	  
	  buflen = (int) domlen + 5;

	  memset(&n, 0, sizeof(n));

	  n.host = buf + 5;
	  n.len = domlen;
	  n.bev = bev;
	  
	  resolvecb(&n);

	  if (status == DNS_OK) {

	    /* And extract 2 bytes port as well */
	    memcpy(portbuf, buf + buflen, 2);
	    port = portbuf[0]<<8 | portbuf[1];

	    n.sin.sin_family = AF_INET;
	    n.sin.sin_port = htons(port);
	    
	    log_debug(DEBUG, "dns_ok");
	    
	    evbuffer_drain(src, buf_size);
	    
	    if (evutil_inet_ntop(AF_INET,
		 (struct sockaddr_in*)&n.sin.sin_addr, abuf, sizeof(abuf))
		== NULL) {
	      log_err("failed to resolve host");
	      destroycb(bev);
	      return;
	      
	    }

	    log_info("* %s:%d", abuf, port);

	    if (bufferevent_socket_connect(partner,
			   (struct sockaddr*)&n.sin, sizeof(n.sin)) != 0)
	      {
		log_err("connect: failed to connect");
		destroycb(bev);
		return;
	      }

	    status = SCONNECTED;
	  }

	  break;
	default:
	  log_err("unkown atype=%d", buf[3]);
	  destroycb(bev);
	  return;
	}

	if (status == SCONNECTED)
	  {
	    /* wait for target's response and if any data back, send it back
	     * to client 
	     */
	    bufferevent_setcb(bev, remote_readcb, NULL, eventcb, partner);
	    bufferevent_enable(bev, EV_READ|EV_WRITE);
	  }
      }

  } else {
    /* Seems a wrong protocol; get this destroyed. */
    log_err("wrong protocol=%d", buf[0]);
    destroycb(bev);
    return;
  }
}


static void
resolvecb(socks_name_t *s)
{
  if (resolve_host(s) == 0) {
    status = DNS_OK;
  } else {
    destroycb(s->bev);
    return;
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
  
  log_debug(DEBUG, "payload to target=%ld", buf_size);
  bufferevent_write(partner, buf, buf_size);
  evbuffer_drain(src, buf_size);  

  /* wait for target's response and send data back to a client */
  bufferevent_setcb(partner, readcb_from_target, NULL, eventcb, bev);
  bufferevent_enable(partner, EV_READ|EV_WRITE);
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
    bufferevent_disable(bev, EV_WRITE);    
  }
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
  struct evbuffer *src = bufferevent_get_input(bev);
  size_t buf_size = evbuffer_get_length(src);

  u8 payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};    
  u8 buf[buf_size];

  evbuffer_copyout(src, buf, buf_size);  
  evbuffer_drain(src, buf_size);
      
  socks_cmd_e cmd = buf[1];

  /* Check if version is correct and status is equal to INIT */
  if (status == SINIT && buf[0] == SOCKS_VERSION)
    {      
      status = SWAIT;
      
      /* parse socks header */
      switch (cmd) {
      case CONNECT:
      case BIND:
	break;
      case UDPASSOC:
	log_warn("udp associate");
	break;
      default:
	log_warn("unkonw cmd=%d", buf[1]);
	destroycb(bev);
	return;
      }  
    }

  if (bufferevent_write(partner, buf, buf_size) <0) {
    log_err("bufferevent_write");      
    destroycb(bev);
    return;    
  }

  /* set callbacks and wait for server response */  
  bufferevent_setcb(partner, readcb_from_target, NULL, eventcb, bev);
  bufferevent_enable(partner, EV_WRITE|EV_READ);
  bufferevent_enable(bev, EV_WRITE);
}


static void
readcb_from_target(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  struct evbuffer *src;
  size_t buf_size;
  
  src = bufferevent_get_input(bev);
  buf_size  = evbuffer_get_length(src);
  u8 buf[buf_size];
  
  if (!partner) {
    log_debug(DEBUG, "readcb_from_target drain");    
    evbuffer_drain(src, buf_size);
    return;
  }

  evbuffer_copyout(src, buf, buf_size);
  evbuffer_drain(src, buf_size);  
  log_debug(DEBUG, "drained=%ld", buf_size);
    
  if (bufferevent_write(partner, buf, buf_size) < 0) {
    log_err("bufferevent_write");      
    destroycb(bev);
    return;    
  }
}


static void
drained_writecb(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner = ctx;
  
  log_debug(DEBUG, "EXCEEDING MAX_OUTPUT %ld",
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
	  log_err("cannot connect to the server");
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
syntax(void)
{  
  printf("Usage: esocks [OPTIONS] ...\n");
  puts("");
  printf("Options\n");
  printf(" Server options:\n");
  printf("  -s --server_addr  server address\n");
  printf("  -p --server_port  server port\n");  
  printf("  -u --local_addr   local address\n");
  printf("  -j --local_port   local port\n");
  printf("  -k --password     password to encrypt/decrypt\n");
  printf("  -w --worker       worker number\n");
  printf("  -b --backend      backend to use\n");
  printf("  -r --limit-rate   maximum packet rate in bytes\n");
  printf("  --local           run local mode\n");  
  exit(1);
}


struct options {
  const char *server_addr;    
  const char *server_port;    
  const char  *local_addr;
  const char  *local_port;        
  const char    *password;
  const char     *backend;  
  const char      *worker;
  const char        *rate;
};


static void
parse_opt(struct options *opt, int c, char **v)
{
  int cc;
  
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

      cc = getopt_long(c, v, "vls:p:u:j:k:", lp, &opt_index);
      
      if (cc == -1)
	break;

      switch(cc)
	{
	case 0:
	  if (lp[opt_index].flag != 0) break;
	case 's': opt->server_addr = optarg; break;
	case 'p': opt->server_port = optarg; break;
	case 'u': opt->local_addr  = optarg; break;
	case 'j': opt->local_port  = optarg; break;
	case 'k': opt->password    = optarg; break;
	case 'l':      yes_this_is_local; break;
	case 'v':         verbose_flag++; break;
	case '?':               syntax(); break;
	}
    }  
}


int
main(int c, char **v)
{    
  int cc;
  char opt;    
  struct options o;

  memset(&o, 0, sizeof(o));
  parse_opt(&o, c, v);
  
  if (yes_this_is_local) {
    
    if (!(o.local_port && o.local_addr && o.password &&
	  o.server_addr && o.server_port))  
      syntax();
    
  } else { /* forward server */
    
    if (!(o.server_addr && o.server_port && o.password))
      syntax();
    
  }
  
  struct evconnlistener               *listener;  
  static struct sockaddr_storage listen_on_addr;
  static struct sockaddr_storage   forward_addr;  
  struct event                    *signal_event;
  int                                      mode;
  int                                      port;
  int      socklen = sizeof(struct sockaddr_in);
  const char  *levent_ver = event_get_version();  
  
  event_set_fatal_callback(app_fatal_cb);

  memset(&listen_on_addr, 0, sizeof(listen_on_addr));
  memset(&forward_addr, 0, sizeof(forward_addr));
  
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
	  
	}
      
      listener = evconnlistener_new_bind(base, acceptcb,
	    (struct sockaddr*)&forward_addr,
	    LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC,
		           -1, (struct sockaddr*)&listen_on_addr, socklen);
      
      log_info("server is up and running %s:%s connecting %s:%s",
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
	  sin->sin_family = AF_INET; /* TODO IPv6 */
      
	  if (evutil_inet_pton(AF_INET, o.server_addr, &sin->sin_addr)<0)
	    syntax();

#ifdef FAST_OPEN /* Experimental */
	  
 	  int fd;
 	  int optval = 5;
 	  
 	  fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
 
 	  if (fd == -1)
 	    log_errx(1, "fd");
   
 	  if (setsockopt(fd, SOL_TCP, TCP_FASTOPEN, (void*)&optval, sizeof(optval))<0)
 	    log_errx(1, "sockopt");
 
 	  /* TODO: error check */
 	  evutil_make_listen_socket_reuseable(fd);
 	  evutil_make_listen_socket_reuseable_port(fd);
 	  evutil_make_tcp_listen_socket_deferred(fd);
 	  bind(fd, (struct sockaddr*)&listen_on_addr, sizeof(listen_on_addr));
 
 	  listener = evconnlistener_new(base, acceptcb, NULL,
			LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC, -1, fd);
 	  log_info("fastopen enabled");
#else

	  /* Ready for forward connections from clients */
	  listener = evconnlistener_new_bind(base, acceptcb, NULL,
	     LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC,
					     -1, (struct sockaddr*)&listen_on_addr,
					     socklen);

#endif /* FAST_OPEN */	  
	}

      log_info("server is up and running %s:%s", 
		  o.server_addr, o.server_port);      
    }

  if (!listener) {   
    log_err("bind");  
    event_base_free(base);
    return 1;
  }

  if (DEBUG == 1) log_debug(DEBUG, "DEBUG MODE");
  
  signal_event = event_new(base, SIGINT,
			   EV_SIGNAL|EV_PERSIST, signal_func, (void*)base);
  
  if (!signal_event || event_add(signal_event, NULL))
    log_errx(1, "Cannot add a signal_event");
  
  log_info("`%s` mode",
	      yes_this_is_local ? "local" : "remote");
  log_info("Libevent version: %s", levent_ver);
  
  event_base_dispatch(base);
  event_base_free(base);
  
  return 0;
}
