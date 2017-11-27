/* 
   Simple socks5 proxy server with Libevent 
   Author Xun   
   2017

   server replies:

   +----+-----+-------+------+----------+----------+
   |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
   +----+-----+-------+------+----------+----------+
   | 1  |  1  | X'00' |  1   | Variable |    2     |
   +----+-----+-------+------+----------+----------+

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
#include <fcntl.h>

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
  puts("* version is wrong");
  bufferevent_trigger_event(bev, BEV_EVENT_ERROR, 0);  
}

static void
syntax(void)
{
  printf("evsocks [--help] [-v verbose] [-h host] [-p port] [-u user] [-q password] [-c client mode] [-s server mode]\n");
  exit(EXIT_SUCCESS);
}

void
event_func(struct bufferevent *bev, short what, void *ctx)
{

  struct bufferevent *associate = ctx;
  
  if (what & (BEV_EVENT_EOF|BEV_EVENT_CONNECTED|BEV_EVENT_WRITING|BEV_EVENT_READING|BEV_EVENT_ERROR)) {
    
    /* version is wrong, host unreachable or just one of annoying requests */
    /* TODO:                          */
    /*   clean up buffers more gently */

    if (what & BEV_EVENT_ERROR) {
      if (errno)
	perror("connection error, version is wrong or some of other causes");
      bufferevent_free(bev);
      bufferevent_free(associate);
      puts("* freed");
    }

    if (what & BEV_EVENT_EOF) {
      puts("* reached EOF");
      
      bufferevent_free(bev);
      bufferevent_free(associate);
      puts("* freed");
    }

    if (what & BEV_EVENT_CONNECTED) {
      puts("* connected");
      /* ready for next event */
      bufferevent_setcb(bev, async_read_from_target_func, NULL, event_func, associate);
    }
    if (what & BEV_EVENT_READING) {      
      puts("* read");      
    }
    if (what & BEV_EVENT_WRITING) {
      if (errno)
	perror("* connection error");
    }
  }
}

static void
async_read_func(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *associate = ctx; /* we will have a talk with an associate */
  struct evbuffer *src;
  static struct addrspec *spec; /* this will hold spec til my associate is ready to talk */
  ev_ssize_t evsize; /* we will store buffer size here */
  size_t buf_size; /* how many bytes read so far? */
  unsigned char payload[12] = {5, 0, 5, 0, 0, 1, 0, 0, 0, 0, 0, 0}; /* ready for socks payload */
  
  src = bufferevent_get_input(bev);
  buf_size = evbuffer_get_length(src);
    
  unsigned char header[buf_size];
  evsize = evbuffer_copyout(src, header, buf_size);

  if (header[0] == SOCKS_VERSION) {
    
    puts("* initialize service");
    
    /* start to parse data */
    switch (header[1]) {

    case CONNECT:
      evbuffer_drain(src, 3); /* drain 3 bytes */
      spec = handle_connect(bev, header, evsize);      
      break;
    case BIND:
      evbuffer_drain(src, 4); /* drain 4 bytes */
      spec = handle_connect(bev, header, evsize);            
      break;
    case UDPASSOC:
      puts("* udp associate not supported");
      break;
    default:
      fprintf(stderr, "** unknown command");
      status = SDESTORY;
    }

    /* say ok to out associate */
    if (bufferevent_write(bev, payload, 12)<0) {
      fprintf(stderr, "** async_read_func._write");    
    }

    /* wait a sec if buffer size if short enough */
    if (evsize <= 4) {
      status = SWAIT;      
    } else {
      status = SWRITE;
    }

    switch (status) {
    case SWAIT:
      return;
    case SWRITE:
    case SREAD:
      break;
    }
    
  } else if (associate && status == SWRITE) {
    printf("* Our associate is ready for work.\n");

    unsigned char *reqbuf;
    reqbuf = (unsigned char*)malloc(evsize); /* pull a new buffer */
    reqbuf = header;
    printf("* payload=%ld\n", evsize);
    
    bufferevent_setcb(associate, NULL, NULL, event_func, bev); /* set up callbacks */
    bufferevent_enable(associate, EV_READ|EV_WRITE);
    
    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    
    target.sin_family = AF_INET;       
    target.sin_addr.s_addr = (*spec).s_addr;
    target.sin_port = htons((*spec).port);

    debug_addr(spec); /* debug address */

    if (bufferevent_socket_connect(associate,
				   (struct sockaddr*)&target, sizeof(target))<0){
      perror("_write");
      status = SDESTORY;
    }

    /* invoke next event */
    /* bufferevent_trigger_event(bev, BEV_EVENT_CONNECTED, 0);   */
    
    if (bufferevent_write(associate, reqbuf, evsize)<0){
      perror("* _write");
      status = SDESTORY;      
    }
    
    puts("* sent");
    
  } else if (status == SDESTORY) {
    handle_perpetrators(associate);    
  }else {
    /* handle annoying requests */
    handle_perpetrators(associate);
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

  printf("* says=%ld bytes\n", evsize);
  if (bufferevent_write(associate, buffer, evsize)<0) {
    fprintf(stderr, "** async_read_from_target_func.bufferevent_write\n");
    bufferevent_trigger_event(associate, BEV_EVENT_ERROR, 0);
  }
  // bufferevent_trigger_event(associate, BEV_EVENT_EOF, 0);
}

static void
accept_func(struct evconnlistener *listener,
	    evutil_socket_t fd,
	    struct sockaddr *a, int slen, void *p)
{
  struct bufferevent *src, *dst;
  src = bufferevent_socket_new(base, fd,
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  
  /* note: dst's fd should be -1 since we do not want it to connect yet */
  dst = bufferevent_socket_new(base, -1,
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  
  bufferevent_setcb(src, async_read_func, NULL, event_func, dst);
  bufferevent_enable(src, EV_READ|EV_WRITE);
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
  // struct evconnlistener *listener;
  struct evconnlistener *listener;  
  static struct sockaddr_storage listen_on_addr;

  memset(&o, 0, sizeof(o));

  if (argc < 2) {
    syntax();
  }

  while ((opt = getopt(argc, argv, "vhp:uqcs")) != -1) {
    switch (opt) {
    case 'v': ++verbose; break;
    case 'h': o.host = optarg; break;
    case 'p': o.port = optarg; break;
    default: fprintf(stderr, "Unknow option %c\n", opt); break;      
    }
  }

  if (o.host == NULL) {
    o.host = "0.0.0.0";
  }
  if (o.port == NULL) {
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
    return 1;
  }

  listener = evconnlistener_new_bind(base, accept_func, NULL,
				     LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC,
				     -1, (struct sockaddr*)&listen_on_addr, socklen);
  if (!listener) {
    perror("evconnlistener_new_bind()");
    event_base_free(base);
    return 1;
  }

  printf("* %s:%s\n", o.host, o.port);
  printf("* level=%d\n", verbose);

  event_base_dispatch(base);
  event_base_free(base);
  exit(EXIT_SUCCESS);
}
