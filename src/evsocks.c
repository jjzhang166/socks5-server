/* 
   Simple proxy server with Libevent 
   Author Xun   
   2017
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
#include "evsocks.h"
#include "slog.h"

static struct event_base *base;

static int status;

static void
handle_perpetrators(struct bufferevent *bev, ...)
{
  /* let's destory this buffer */
  puts("* version is so wrong");
  bufferevent_trigger_event(bev, BEV_EVENT_ERROR, 0);  
}

static void
syntax(void)
{
  printf("evsock [-h host] [-p port]\n");
  exit(EXIT_SUCCESS);
}

static void
event_func(struct bufferevent *bev, short what, void *ctx)
{  
  
  if (what & (BEV_EVENT_EOF|BEV_EVENT_CONNECTED|
	      BEV_EVENT_READING|BEV_EVENT_READING)) {
    
    struct bufferevent *associate = ctx;
    
    /* version is wrong, host unreachable or just one of annoying requests
    * TODO:                         
    *   clean up buffers more gently 
    *
    */
    if ((what & BEV_EVENT_ERROR && status == SDESTORY)) {
      if (errno)
	logger_err("event_func");
      bufferevent_free(bev);
      if (associate)
	bufferevent_free(associate);
      logger_info("freed");
    }
    
    if (what & BEV_EVENT_EOF) {
      logger_debug("reached EOF");
      bufferevent_free(bev);
    }
  }
}

static void
socks_init_func(struct bufferevent *bev, void *ctx)
{
  /* we will have a talk with our associate */
  struct bufferevent *associate = ctx;
  struct evbuffer *src;
  /* store copied buffer size here */
  ev_ssize_t evsize;
  /* store size from bev */
  size_t buf_size;

  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
    
  unsigned char reqbuf[buf_size];
  evsize = evbuffer_copyout(src, reqbuf, buf_size);

  /* socks payload */
  unsigned char payload[2] = {5, 0};
  if (reqbuf[0] == SOCKS_VERSION) {
    
    logger_info("getting a request");
    
    status = SINIT;
  /* TODO: in case, spec is null, return an error message to a client */
    /* say ok to our associate */
    if (bufferevent_write(bev, payload, 2)<0) {
      logger_err("socks_init_func.bufferevent_write");
      status = SDESTORY;
    }
    
    /* drain first bytes  */
    evbuffer_drain(src, evsize);
    
    bufferevent_setcb(bev, async_read_func, async_write_func, event_func, associate);
    bufferevent_enable(bev, EV_READ|EV_WRITE);    
    return;
  }

  logger_err("wrong protocol=%d", reqbuf[0]);
  handle_perpetrators(bev);
}

static void
async_read_func(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src;  
  static struct addrspec *spec;
  unsigned char *buffer;
  size_t buf_size;
  unsigned char payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
  
  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
  buffer = malloc(buf_size);
  if (evbuffer_copyout(src, buffer, buf_size)<0)
    logger_err("async_read_func.evbuffer_copyout");
 
  if (status == SINIT) {    

    /* parse header */
    switch (buffer[1]) {

    case CONNECT:
      spec = handle_connect(bev, buffer, buf_size);      
      break;

    case BIND:
      spec = handle_connect(bev, buffer, buf_size);            
      break;
      
    case UDPASSOC:
      logger_warn("* udp associate not supported");
      status = SDESTORY;
      break;
      
    default:
      logger_err("unknown command");
      status = SDESTORY;
    }
    
    if (!spec) {
      logger_warn("spec cannot be NULL");
      status = SDESTORY;
    } else {
      bufferevent_enable(bev, EV_WRITE);
      status = SREAD;
      /* get client be ready to write */
      /* connect to a target and set up next event */
      struct sockaddr_in target;
      target.sin_family = AF_INET; /* TODO: v6 */
      target.sin_addr.s_addr = (*spec).s_addr;
      target.sin_port = htons((*spec).port);
      
      debug_addr(spec);
      
      if (bufferevent_socket_connect(associate,
				     (struct sockaddr*)&target, sizeof(target))<0){
	logger_err("failed bufferevevnt_socket_connect");
	payload[1] = HOST_UNREACHABLE;
	if (bufferevent_write(bev, payload, 10)<0) {
	  logger_err("async_read_func.bufferevent_write");
	  status = SDESTORY;
	  return;
	}
      }
      evbuffer_drain(src, buf_size);
      logger_debug("drain=%ld", buf_size);
      return;
    }
  }
  if (status == SREAD) {
    /* make sure we already have a connection and pull the payload from a client */
    if (bufferevent_write(associate, buffer, buf_size)<0) {
      logger_err("async_read_func.bufferevent_write");
      status = SDESTORY;
    }
    logger_debug("wrote to target=%ld bytes", buf_size);
    evbuffer_drain(src, buf_size);
    logger_debug("drain %ld", buf_size);    
    bufferevent_setcb(associate, async_handle_read_from_target,
		      NULL, event_func, bev);
    bufferevent_enable(associate, EV_READ|EV_WRITE);
    return;
  }
  if (status == SDESTORY) {
    logger_debug("destory");
    handle_perpetrators(bev);
  }  
}

static void
async_handle_read_from_target(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src;
  size_t buf_size;
  unsigned char *buffer;
  
  src = bufferevent_get_input(bev);  /* first pull payload from client */
  buf_size  = evbuffer_get_length(src);
  buffer = malloc(buf_size);

  if (associate == NULL) {
    /* client left early?? */
    logger_err("asyn_handle_read_from_target: client left");
    status = SDESTORY;
    return;
  }

  if (status == SREAD) {
    if (evbuffer_copyout(src, buffer, buf_size)<0) {
      logger_err("async_handle_read_from_target.evbuffer_copyout");
      status = SDESTORY;
    }    
    logger_debug("payload to client=%ld", buf_size);
    bufferevent_write(associate, buffer, buf_size);
    evbuffer_drain(src, buf_size);    
  }
}

static void
async_write_func(struct bufferevent *bev, void *ctx)
{
  unsigned char payload[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
  
  if (status == SINIT) {
    if (bufferevent_write(bev, payload, 10)<0) {
      logger_err("async_read_func._write set to SDESTROY");
      status = SDESTORY;    
    }
    logger_debug("async_write_func: wrote");
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
     Both src and dst will have a talk over an evbffer.
  */
  struct bufferevent *src, *dst;
  src = bufferevent_socket_new(base, fd,
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  /* note: dst's fd should be -1 since we do not want it to connect to target yet */
  dst = bufferevent_socket_new(base, -1, 
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  
  assert(src && dst);
  
  bufferevent_setcb(src, socks_init_func, NULL, event_func, dst);
  bufferevent_enable(src, EV_READ|EV_WRITE);
}

static void
signal_func(evutil_socket_t sig_flag, short what, void *ctx)
{
  struct event_base *base = ctx;
  struct timeval delay = {1, 0};
  int sec = 1;
  
  logger_err("*** Caught an interupt signal; exiting cleanly in %d second(s)", sec);
  event_base_loopexit(base, &delay);
}

int
main(int argc, char **argv)
{
  struct options {
    const char *port;
    const char *host;
  };
  struct options o;
  char opt;
  static int verbose;
  
  int socklen, port;  
  struct evconnlistener *listener;  
  static struct sockaddr_storage listen_on_addr;
  struct event *signal_event;

  if (argc < 2 || (strcmp(argv[1], "--help") == 0)) {
    syntax();
  }

  memset(&o, 0, sizeof(o));

  while ((opt = getopt(argc, argv, "h:p:vuqcs")) != -1) {
    switch (opt) {
    case 'v': ++verbose; break;
    case 'h': o.host = optarg; break;
    case 'p': o.port = optarg; break;
    default: fprintf(stderr, "Unknow option %c\n", opt); break;      
    }
  }

  if (!o.host) {
    o.host = "0.0.0.0";
  }
  if (!o.port) {
    syntax();
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
    if (inet_pton(AF_INET, o.host, &((*sin).sin_addr)) == -1)
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

  signal_event = event_new(base, SIGINT, EV_SIGNAL|EV_PERSIST, signal_func, (void*)base);

  if (!signal_event || event_add(signal_event, NULL)) {
    logger_errx(1, "Cannot add a signal_event");
    // exit(EXIT_FAILURE);
  }

  event_base_dispatch(base);
  event_base_free(base);
  exit(EXIT_SUCCESS);
}
