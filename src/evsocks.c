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
static void handle_perpetrators(struct bufferevent *bev);
static void signal_func(evutil_socket_t sig_flag, short what, void *ctx);


static void
syntax(void)
{
  printf("Usage: esocks [options...]\n");
  printf("Options:\n");
  printf("  -a authentication e.g, -a username:password\n");
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
handle_perpetrators(struct bufferevent *bev)
{
  /* let's destroy this buffer */
  logger_err("version is so wrong");
  logger_info("status=%d", status);
  bufferevent_trigger_event(bev, BEV_EVENT_ERROR, 0);  
}

static void
event_func(struct bufferevent *bev, short what, void *ctx)
{  

  struct bufferevent *associate = ctx;
      
  if (what & (BEV_EVENT_EOF|BEV_EVENT_ERROR|BEV_EVENT_CONNECTED)) {
    
    /* version is wrong, host unreachable or just one of annoying requests
    * TODO:
    *   clean up buffers more gently 
    *   find out what causes errors */
    if ((what & BEV_EVENT_ERROR && status == SDESTROY)) {
      if (errno)
	logger_err("event_func");
      bufferevent_free(bev);
    if (associate)
      bufferevent_free(associate);
    logger_info("buffer freed");
    }

    if (what & BEV_EVENT_ERROR) {
      logger_err("event_func with no SDESTROY flag");
    }

  if (what & BEV_EVENT_EOF) {
    logger_debug(verbose, "reached EOF");
      bufferevent_free(bev);
      bufferevent_free(associate);
    }
  }
}

static void
socks_init_func(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src;
  /* store copied buffer size here */
  ev_ssize_t evsize;
  /* store size from bev */
  size_t buf_size;

  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
    
  ev_uint8_t reqbuf[buf_size];
  evsize = evbuffer_copyout(src, reqbuf, buf_size);
  
  /* socks payload */
  ev_uint8_t payload[2] = {5, 0};

  if (auth) {
    payload[1] = 2;
  }
  
  if (reqbuf[0] == SOCKS_VERSION) {
    
    logger_info("getting a request");

    status = SINIT;
    
    /* write message to clients */
    if (bufferevent_write(bev, payload, 2)<0) {
      logger_err("socks_init_func.bufferevent_write");
      status = SDESTROY;
    }

    if (auth) {
      logger_debug(verbose, "callback to auth_func");
      bufferevent_setcb(bev, async_auth_func,
			NULL, event_func, associate);
      return;
    }
    
    /* drain first bytes  */
    evbuffer_drain(src, evsize);
      
    bufferevent_setcb(bev, async_read_func,
		      async_write_func, event_func, associate);

    bufferevent_enable(bev, EV_READ|EV_WRITE);    
    return;
  }

  status = SDESTROY;
  logger_err("wrong protocol=%d", reqbuf[0]);
  handle_perpetrators(bev);
}

static void
async_auth_func(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src;
  ev_uint8_t *buffer;
  size_t buf_size;
  ev_uint8_t payload[2] = {5, 0};
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
    logger_err("async_read_func.evbuffer_copyout");

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
    status = SDESTROY;
  }
  
  if (status == SDESTROY) {
    logger_info("destroy");
    handle_perpetrators(bev);
    return;
  }
  
  logger_debug(verbose, "auth method=%d;userl=%d;passwdlen=%d", method, userlen, passwdlen);
  
  user = calloc(userlen, sizeof(char));  /* allocate empty data */
  passwd = calloc(passwdlen, sizeof(char)); /* allocate empty data */
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
      status = SDESTROY;
    }

  } else {
    
    status = SDESTROY;
    
  }

  if (status == SDESTROY) {
    
    logger_info("destroy");
    handle_perpetrators(bev);

  } else {  
  
    /* drain first bytes  */
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
  ev_uint8_t *buffer;
  size_t buf_size;
  ev_uint8_t payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
  
  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
  buffer = calloc(buf_size, sizeof(ev_int8_t));
  
  if (evbuffer_copyout(src, buffer, buf_size)<0)
    logger_err("async_read_func.evbuffer_copyout");

  if (auth) {
    /* now only support username/password authentication */
    payload[1] = 2;
  }
  
  if (status == SINIT) {    

    /* parse this header */
    switch (buffer[1]) {
    case CONNECT:
      spec = handle_connect(bev, buffer, buf_size);      
      break;
    case BIND:
      spec = handle_connect(bev, buffer, buf_size);            
      break;
    case UDPASSOC:
      logger_warn("udp associate is not supported");
      payload[1] = NOT_SUPPORTED;
      status = SDESTROY;
      break;
    default:
      logger_err("unknown command %d", buffer[1]);
      status = SDESTROY;
      spec = NULL;
    }

    if (status == SDESTROY) {
      logger_info("destroy");
      handle_perpetrators(bev);    
    }  
    
    if (!spec) {
      
      logger_warn("spec cannot be NULL");
      status = SDESTROY;
      
    } else {
      bufferevent_enable(bev, EV_WRITE);
      status = SREAD;
      /* get this client ready to write */
      /* connects to a target and sets up next events */
      struct sockaddr_in target;
      target.sin_family = AF_INET; /* TODO: v6 */
      target.sin_addr.s_addr = (*spec).s_addr;
      target.sin_port = htons((*spec).port);

      if (bufferevent_socket_connect(associate,
	     (struct sockaddr*)&target, sizeof(target))<0){
	
	logger_err("is failed bufferevevnt_socket_connect");

	status = SDESTROY;
	
	payload[1] = HOST_UNREACHABLE;
	
	if (bufferevent_write(bev, payload, 10)<0) {
	  logger_err("async_read_func.bufferevent_write");
	}
      }
  
      evbuffer_drain(src, buf_size);
      logger_debug(verbose, "socket_connect and drain=%ld", buf_size);
      return;
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
    logger_info("destroy");
    handle_perpetrators(bev);    
    return;
  }  
}

static void
async_handle_read_from_target(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src;
  size_t buf_size;
  ev_uint8_t *buffer;
  
  src = bufferevent_get_input(bev);  /* first pull payload from this client */
  buf_size  = evbuffer_get_length(src);
  buffer = calloc(buf_size, sizeof(ev_uint8_t));

  if (!associate) {
    /* client left early?? */
    logger_err("asyn_handle_read_from_target: client left");
    status = SDESTROY;
  }

  if (status == SREAD) {
    if (evbuffer_copyout(src, buffer, buf_size)<0) {
      logger_err("async_handle_read_from_target.evbuffer_copyout");
      status = SDESTROY;
    } else {
    logger_debug(verbose, "payload to client=%ld", buf_size);

    if (bufferevent_write(associate, buffer, buf_size)<0) {
      logger_err("async_handle_read_from_target");
      status = SDESTROY;
    }
    evbuffer_drain(src, buf_size);
    }
  }  
}

static void
async_write_func(struct bufferevent *bev, void *ctx)
{
  ev_uint8_t payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};

  if (!bev)
    status = SDESTROY;
  
  if (status == SINIT) {
    if (bufferevent_write(bev, payload, 10)<0) {
      logger_err("async_read_func._write set to SDESTROY");
      status = SDESTROY;    
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
     Both src and dst will have a talk over an bufferevent.
  */
  struct bufferevent *src, *dst;

  src = bufferevent_socket_new(base, fd,
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  /* note: dst's fd should be -1 since we do not want it to connect to target yet */
  dst = bufferevent_socket_new(base, -1, 
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  
  bufferevent_setcb(src, socks_init_func, NULL, event_func, dst);
  bufferevent_enable(src, EV_READ|EV_WRITE);
}

int
main(int argc, char **argv)
{

  /* ignore SIGPIPE event */
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    logger_err("signal(SIGPIPE, SIG_IGN..");
  
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

  /* allocate mem for sockaddr_in */
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
