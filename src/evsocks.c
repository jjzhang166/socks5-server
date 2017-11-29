/* 
   Simple socks5 proxy server with Libevent 
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

static struct event_base *base;

static int status;

static void
handle_perpetrators(struct bufferevent *bev)
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
    
    /* version is wrong, host unreachable or just one of annoying requests */
    /* TODO:                          */
    /*   clean up buffers more gently */
    if ((what & BEV_EVENT_ERROR && status == SDESTORY) || status == STOP) {
      printf("* status=%d\n", status);
      if (errno)
	perror("** error");
      bufferevent_free(bev);
      if (associate)
	bufferevent_free(associate);
      puts("* freed");
    }

    if (what & BEV_EVENT_CONNECTED) {
      puts("* connected");
      /* ready for next event */
      bufferevent_setcb(bev,
			async_read_from_target_func,
			NULL, event_func, associate);
    }

    if (((what & BEV_EVENT_EOF)) && (status == SFINISHED)) {
      puts("** EOF");
      printf("* status=%d\n", status);
    }
    
    if (what & BEV_EVENT_EOF) {
      puts("* just EOF");
    }

    if (what & BEV_EVENT_WRITING)
      puts("* just WRITING");
  }
}

/* jsut for drain? for cleaning-up leftover?? */
static void
drain_and_free_func(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src;
  size_t buf_size;
  
  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);

  printf("* draining a packet=%ld\n", buf_size);
  if (buf_size>0) {
    evbuffer_drain(src, buf_size);
  }
  if (associate)
    bufferevent_free(bev);
}

static void
async_write_func(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src;
  size_t buf_size;
  
  src = bufferevent_get_input(associate);
  buf_size = evbuffer_get_length(src);
    
  printf("* to client=%ld\n", buf_size);
  
  /* handle leftover here */
  switch (status) {
  case SREAD:
    printf("* async_write_func drains %ld\n", buf_size);
    evbuffer_drain(src, buf_size);
    bufferevent_enable(associate, EV_READ);
    break;
  case SWRITE:
    printf("* async_write_func server writes=%ld\n", buf_size);
    break;
  case SFINISHED:
    puts("** connection cleaning up");
    /* bufferevent_trigger_event(bev, BEV_EVENT_EOF, 0); */
    if (buf_size == 0) {
      bufferevent_free(associate);
      bufferevent_free(bev);
    }
    break;
  }
}

static void
async_read_func(struct bufferevent *bev, void *ctx)
{
  /* we will have a talk with our associate */
  struct bufferevent *associate = ctx;
  struct evbuffer *src;
  static struct addrspec *spec;
  /* store copied buffer size here */
  ev_ssize_t evsize;
  /* store size from bev */
  size_t buf_size;

  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
    
  unsigned char reqbuf[buf_size];
  evsize = evbuffer_copyout(src, reqbuf, buf_size);

  /* socks payload */
  unsigned char payload[12] = {5, 0, 5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
  /* in case, failed for some reasons */
  unsigned char failed[10];

  if (reqbuf[0] == SOCKS_VERSION) {

    puts("** init **");
    /* start to parse data */
    switch (reqbuf[1]) {

    case CONNECT:
      evbuffer_drain(src, 3); /* drain 3 bytes */
      spec = handle_connect(bev, reqbuf, evsize);      
      break;

    case BIND:
      evbuffer_drain(src, 4); /* drain 4 bytes */
      spec = handle_connect(bev, reqbuf, evsize);            
      break;
      
    case UDPASSOC:
      puts("* udp associate not supported");
      status = SDESTORY;
      break;
      
    default:
      fprintf(stderr, "** unknown command");
      status = SDESTORY;
    }

    /* say ok to our associate */
    if (bufferevent_write(bev, payload+2, 2)<0) {
      fprintf(stderr, "** async_read_func._write");
      status = SDESTORY;
    }

    /* make sure we have a spec and then can send the rest of buffer */
    if (spec) {
      
      debug_addr(spec); /* debug address */
      
      if (bufferevent_write(bev, payload+4, 8)<0) {
      	fprintf(stderr, "** async_read_func._write");
      	status = SDESTORY;
      }
    }
    
    /* wait til buffer size is enough to talk to target */
    if (evsize <= 4) {
      status = SWAIT;
      return; /* return and wait for next event */
    } else {
      status = SWRITE;
    }

  } else if (associate && status == SWRITE) {

    puts("* Our associate is ready for work");    
    printf("* payload=%ld\n", evsize);

    bufferevent_setcb(associate, NULL, NULL, event_func, bev); /* set up callbacks */
    bufferevent_enable(associate, EV_READ);

    struct sockaddr_in target;
    
    memset(&target, 0, sizeof(target));
    
    target.sin_family = AF_INET;       
    target.sin_addr.s_addr = (*spec).s_addr;
    target.sin_port = htons((*spec).port);

    if (bufferevent_socket_connect(associate,
				   (struct sockaddr*)&target, sizeof(target))<0){
      memcpy(failed, payload+2, 10);
      failed[1] = HOST_UNREACHABLE;
      if (bufferevent_write(bev, failed, 10)<0) {
      	fprintf(stderr, "** async_read_func._write");    
      }
      perror("_write");
      status = SDESTORY;
    }

    /* write out data to target  */
    if (bufferevent_write(associate, reqbuf, evsize)<0){      
      status = SDESTORY;
    }
    
    if (status == SDESTORY) {
      handle_perpetrators(associate);
    }

  } else if (status == SREAD) { /* handle leftovers  */

    printf("* this must be tls, handle leftover %ld\n", evsize);    
    printf("* status=%d\n", status);
    
    puts("* associate is present and I am writing data to it");
    
    if (bufferevent_write(associate, reqbuf, evsize)<0) {
      fprintf(stderr, "** async_read_func._write");    
    }

  } else {
    /* this buffer must be purged */
    printf("** status=%d\n", status);
    puts("** this client seems wrong");
    handle_perpetrators(bev);
  }
}

static void
async_read_from_target_func(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx;
  struct evbuffer *src;
  ev_ssize_t evsize; /* we will store buffer size here */
  size_t buf_size; /* how many bytes read so far? */

  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
    
  unsigned char buffer[buf_size];
  evsize = evbuffer_copyout(src, buffer, buf_size);
  
  printf("* server spoke=%ld bytes\n", evsize);
  printf("* status=%d\n", status);
  
  if (status == SWRITE) {
    /* disable once and then enable it again when status is SREAD */
    bufferevent_disable(bev, EV_READ);
  }

  else if (status == SREAD) {
    /* drain buffer from the other side */
    /* bufferevent_enable(associate, EV_WRITE); */
    printf("* drain called: %ld\n", buf_size);
    evbuffer_drain(src, buf_size);
  }
  
  /* 
     make sure status is set to SREAD. 
     Otherwise, clients will leave early.
     
   */
  
  puts("* set to SREAD");
  status = SREAD;
  
  puts("* writing to socket");  
  if (bufferevent_write(associate, buffer, evsize)<0) {
    fprintf(stderr,
	    "** async_read_from_target_func.bufferevent_write\n");
     /* operation aborted */
    bufferevent_trigger_event(associate, BEV_EVENT_ERROR, 0);
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
  
  bufferevent_setcb(src, async_read_func, async_write_func, event_func, dst);
  bufferevent_enable(src, EV_READ);
}

static void
signal_func(evutil_socket_t sig_flag, short what, void *ctx)
{
  struct event_base *base = ctx;
  struct timeval delay = {1, 0};
  int sec = 1;
  
  printf("*** Caught an interupt signal; exiting cleanly in %d second(s)\n", sec);
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
    perror("event_base_new()");
    exit(EXIT_FAILURE);
  }

  listener = evconnlistener_new_bind(base, accept_func, NULL,
		     LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC,
				     -1, (struct sockaddr*)&listen_on_addr, socklen);
  if (!listener) {
    perror("evconnlistener_new_bind()");
    event_base_free(base);
    exit(EXIT_FAILURE);    
  }
  
  printf("\n* %s:%s *\n", o.host, o.port);
  printf("* level=%d\n", verbose);

  signal_event = event_new(base, SIGINT, EV_SIGNAL|EV_PERSIST, signal_func, (void*)base);

  if (!signal_event || event_add(signal_event, NULL)) {
    fprintf(stderr, "** Cannot add a signal_event\n");
    exit(EXIT_FAILURE);
  }

  event_base_dispatch(base);
  event_base_free(base);
  exit(EXIT_SUCCESS);
}
