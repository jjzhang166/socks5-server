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

static int status;

static struct event_base *base;

static void
read_func(struct bufferevent *bev, void *ctx)
{
  struct evbuffer *src;
  // struct bufferevent *new_bev = ctx; /* should have a partner */
  ev_ssize_t esize;
  size_t len;

  static struct addrspec *spec;

  unsigned char *reqbuf; /* reqbuf will data send by clients */
  
  /* this is definitely temporary */
  unsigned char payload[12] = {5, 0, 5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
  
  src = bufferevent_get_input(bev);
  len = evbuffer_get_length(src);
  
  unsigned char buffer[len];
  esize = evbuffer_copyout(src, buffer, len);

  if (buffer[0] == SOCKS_VERSION) {
    printf("  [INFO: get request]\n");
    printf("  [INFO: buf size=%ld]\n", esize);
    
    /* start to parse data */
    switch (buffer[1]) {
      
    case CONNECT:
      puts("* connect");
      evbuffer_drain(src, 3); /* cut 3 bytes */
      spec = handle_connect(bev, buffer, esize);
      break;
      
    case BIND:
      puts("* bind");
      evbuffer_drain(src, 4); /* cut 4 bytes */
      spec = handle_connect(bev, buffer, esize);      
      break;
      
    case UDPASSOC:
      puts("* udp associate");
      // handle_udpassoc();
      break;
      
    default:
      status = SDESTORY;
      fprintf(stderr, "[ERROR: read_func.switch cmd not supported]\n");
    }
    /* send an initial reply */
    if (bufferevent_write(bev, payload, 12)<0) { /* send out ack reply */
      fprintf(stderr, "[ERROR read_func.bufferevent_write]\n");
      bufferevent_free(bev);
      bufferevent_disable(bev, EV_READ);    
    }
    
    if (esize <= 4) {
      puts("* len(buffer) <= 4");
      status = SWAIT; /* yet another socks requests */
    } else {
      printf("* status=%d\n", status);
    }
    
    switch (status) {
      
    case SREAD:
      puts("[INFO: read_func.SREAD]");
      debug_addr(spec);
      status = SWRITE;
      break;
    
    case SWAIT: /* swaits does nothing other than wait */
      puts("[INFO: drain then read_func.SWAIT]");
      status = SREAD;
      break;
    
    case SWRITE:      
      puts("[INFO: read_func.SWRTIE]");      
      break;
    
    case SDESTORY:
      puts("[INFO: read_func.SDESTORY]");
      bufferevent_free(bev);
      bufferevent_disable(bev, EV_READ);
      break;
    }

  } else if (status == SWRITE) {
    
    printf("* spec.family=%d\n", (*spec).sin_family);

    printf("* %ld\n", len);
    
    struct bufferevent *new_bev;

    reqbuf = (unsigned char*)malloc(esize); /* data to send to target */
    reqbuf = buffer;

    if ((*spec).sin_family == AF_INET) { /* in case IPv4 */
      
      struct sockaddr *sa;
      struct sockaddr_in target;
      
      memset(&target, 0, sizeof(target)); 
      target.sin_family = AF_INET;       
      target.sin_addr.s_addr = (*spec).s_addr;
      target.sin_port = (*spec).port;
      
 // new_bev = bufferevent_socket_new(base, -1,
 // 				       BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);      
      
      /* do address debug */
      char debug[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(target.sin_addr), debug, INET_ADDRSTRLEN);
      printf("* aa %s\n", debug);
      /* do address debug */      

      sa = (struct sockaddr*)&target;

      int sockfd, sent;
      sockfd = socket(AF_INET, SOCK_STREAM, 0);
      evutil_make_socket_nonblocking(sockfd);      
      if (connect(sockfd, sa, sizeof target)<0 )
	{
	  perror("connect");
	}
      
      bufferevent_trigger_event(bev, BEV_EVENT_CONNECTED, 0);            
      sent = send(sockfd, reqbuf, len, 0);
      printf("* sent %d\n", sent);

      bufferevent_setcb(bev, NULL, NULL, on_event_func, NULL);
      bufferevent_enable(bev, EV_WRITE);
      // if (bufferevent_socket_connect(new_bev, sa, sizeof(target))<0) 
      // 	{
      // 	  puts("* freed due to error");	  
      // 	  bufferevent_free(new_bev);	  
      // 	}
      // 
      // bufferevent_trigger_event(new_bev, BEV_EVENT_CONNECTED, 0);
      // 
      // if (bufferevent_write(new_bev, reqbuf, len)<0) 
      // 	{
      // 	  puts(" *  _write freed due to error");
      // 	  bufferevent_free(new_bev);
      // 	}
      // 
      puts("* made a connection");

    } else if ((*spec).sin_family == AF_INET6) { /* in case IPv6 */

      struct sockaddr_in6 ip6addr;
      ip6addr.sin6_family = (*spec).sin_family;

      if (inet_pton(AF_INET6, fetch_addr(spec), &(ip6addr.sin6_addr))<0)
	error_exit("read_func.inet_pton AF_INET6");
      ip6addr.sin6_port = (*spec).port;
      puts("sockaddr_in6 gets ready for connect!!");

    } else { /* in case doamin */
      puts("* address not resolved");
    }

  } else if (status == SDESTORY) {  
    /* do nothing and free memory */
    puts("* read_func.destory");
    bufferevent_free(bev);
    bufferevent_disable(bev, EV_READ|EV_WRITE);
  } else {

    puts("* here");
    
    // bufferevent_free(bev);
    // bufferevent_disable(bev, EV_READ|EV_WRITE);
  }
}

static void
on_read_data(struct bufferevent *bev, void *ctx)
{
  puts("* on_read_data");
}

static void
on_write_data(struct bufferevent *bev, void *ctx)
{
  puts("* on_write_data");
  // bufferevent_write(bev, ctx, sizeof(payload));
}

static void
on_event_func(struct bufferevent *bev, short what, void *ctx)
{
  
  puts("* on_event_func");  
  if (what & (BEV_EVENT_CONNECTED|BEV_EVENT_WRITING|BEV_EVENT_READING)) {
    if (what & BEV_EVENT_CONNECTED) {
      puts("* connected");
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
close_on_flush(struct bufferevent *bev, void *ctx)
{
  struct evbuffer *b = bufferevent_get_output(bev);
  
  if (evbuffer_get_length(b) == 0) {
    puts("close_on_flush.flush");
    bufferevent_free(bev);
  } else {
    puts("data to read close_on_flush");
  }
}

static void
event_func(struct bufferevent *bev, short what, void *ctx)
{
  struct bufferevent *evbuf = ctx;
      
  if (what & (BEV_EVENT_READING|BEV_EVENT_WRITING|BEV_EVENT_ERROR)) {

    if (what & BEV_EVENT_ERROR) {
      if (errno)
	perror("connection error");
    }
    
    if (what & BEV_EVENT_WRITING) {
      // puts("[INFO event_func.WRITING]");
    }
    
    if (what & BEV_EVENT_READING) {
      puts("[INFO event_func.reading]");
    }
    
    if (evbuf) {
      /* Flush all pending data */
      if (evbuffer_get_length(bufferevent_get_output(evbuf))) {
	printf("what sholud I do ...just disable it...\n");
	bufferevent_setcb(evbuf,
	 		  NULL, close_on_flush,
	 		  event_func, NULL);	
	bufferevent_disable(evbuf, EV_READ|EV_WRITE);
      } else {
	/* We have nothing left */	
	bufferevent_free(evbuf);
      }
    }
    bufferevent_free(bev);
  }
}

static void
accept_func(struct evconnlistener *listener,
	    evutil_socket_t fd,
	    struct sockaddr *a, int slen, void *p)
{
  struct bufferevent *src; //, *dst;
  src = bufferevent_socket_new(base, fd,
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  // dst = bufferevent_socket_new(base, fd,
  // 			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);  
  assert(src); // && dst);
  
  bufferevent_setcb(src, read_func, NULL, event_func, NULL);
  // bufferevent_setcb(dst, on_read_data, NULL, on_event_func, src);
  
  bufferevent_enable(src, EV_READ|EV_WRITE);
  // bufferevent_enable(dst, EV_WRITE);  
}

struct addrspec *
handle_addrspec(unsigned char * buffer)
{
  struct addrspec *spec;
  unsigned char atype = buffer[3];
  int buflen, domlen;  
  unsigned long s_addr;
  unsigned char ip4[4];
  uint32_t ipv4;
  unsigned char pb[2]; /* buffer for port */
  unsigned short port;
  spec = malloc(sizeof(struct addrspec));

  switch (atype) {
  case IPV4:
    buflen = 8;
    memcpy(ip4, buffer+4, 4);
    ipv4 =   (uint32_t)ip4[0] << 24|
      (uint32_t)ip4[1] << 16|
      (uint32_t)ip4[2] << 8|
      (uint32_t)ip4[3];
    // char ip[16];
    // sprintf(ip, "%d.%d.%d.%d", ip4[0],ip4[1],ip4[2],ip4[3]);;
    (*spec).ipv4_addr = malloc(4);
    (*spec).ipv4_addr = ip4;    
    
    s_addr = htonl(ipv4);
    (*spec).s_addr = s_addr;
    (*spec).sin_family = AF_INET;
    break;
  case IPV6:
    buflen = 20;
    (*spec)._s6_addr = (unsigned char*)malloc(16);
    (*spec)._s6_addr = buffer + 4; /* jump to 16 bytes address */
    (*spec).sin_family = AF_INET6;
    printf("[INFO: v6=%d]\n", (*spec)._s6_addr[0]);
    break;
  case _DOMAINNAME:
    domlen = buffer[4];
    buflen = domlen + 5;
    (*spec).domain = (unsigned char*)malloc(domlen);
    (*spec).domain = buffer + 5;
    (*spec).sin_family = 3;
    printf("[INFO: domain=%s len=%d]\n", (*spec).domain, domlen);
    break;
  default:
    fprintf(stderr, "[ERROR handle_addrspec.switch Unknown atype]\n");
    return NULL;
  }

  memcpy(&pb, buffer+buflen, sizeof(pb));
  port = pb[0]<<8|pb[1];
  (*spec).port = port;
  
  return spec;
}

struct addrspec *
handle_connect(struct bufferevent *bev, unsigned char *buffer, ev_ssize_t esize)
{
  struct addrspec *spec = malloc(sizeof(struct addrspec));
  
  int i;
  size_t len;
  
  struct evbuffer *src;
  src = bufferevent_get_input(bev);
  len = evbuffer_get_length(src);
    
  printf("\n[INFO:in raw ");
  for (i = 0; i < esize; ++i) {
    printf("%d ", buffer[i]);	
  }
  puts("]");

  if (esize <=4 )
    return NULL; /* nothing to get from this buffer */
  
  spec = handle_addrspec(buffer);
  if ((spec == NULL)) {
    return NULL;
  }
  /* drain a read buffer */
  evbuffer_drain(src, len);

  return spec;
}

static void
debug_addr(struct addrspec *spec)
{
  // /* ip4 and ip6 are for presentation */
  char ip4[INET_ADDRSTRLEN];
  char ip6[INET6_ADDRSTRLEN];
  
  if (spec == NULL) {
    fprintf(stderr, "debug_addr spec is NULL\n");
    return;
  }
  
  /* going to present address */
  switch ((*spec).sin_family) {
  case AF_INET:
    if (!((inet_ntop(AF_INET, &((*spec).s_addr), ip4, INET_ADDRSTRLEN)) == NULL)) {
      printf("[INFO: debug_addr v4=%s:%d]\n", ip4, (*spec).port);
    }
    break;
  case AF_INET6:
    if (!((inet_ntop(AF_INET6, &((*spec)._s6_addr), ip6, INET6_ADDRSTRLEN)) == NULL)) {
      printf("[INFO: debug_addr v6=%s:%d]\n", ip6, (*spec).port);
    }
    break;
  case 3:
    printf("[INFO: debug_addr domain=%s:%d]\n", (*spec).domain, (*spec).port);
    break;
  default:
    fprintf(stderr, "[ERROR: debug_addr Unknow family]\n");
    break;
  }
}

char *
fetch_addr(struct addrspec *spec)
{
  // /* ip4 and ip6 are for presentation */
  char ip4[INET_ADDRSTRLEN];
  char ip6[INET6_ADDRSTRLEN];
  char *buf;
  
  if (spec == NULL) {
    fprintf(stderr, "fetch_addr spec is NULL\n");
    return NULL;
  }
  
  /* going to present address */
  switch ((*spec).sin_family) {
  case AF_INET:
    if (inet_ntop(AF_INET, &((*spec).s_addr), ip4, INET_ADDRSTRLEN)) {
      buf = ip4;
    }
    break;
  case AF_INET6:
    if (inet_ntop(AF_INET6, &((*spec)._s6_addr), ip6, INET6_ADDRSTRLEN)) {
      buf = ip6;
    }
    break;
  case 3:
    printf("[INFO: fetch_addr domain=%s:%d]\n", (*spec).domain, (*spec).port);
    buf = NULL;
    break;
  default:
    fprintf(stderr, "[ERROR: fetch_addr Unknow family]\n");
    buf = NULL;
    break;
  }
  
  printf("* %s\n", buf);
  return buf;
}

static void
syntax(void)
{
  printf("evsocks [--help] [-v verbose] [-h host] [-p port] [-u user] [-q password] [-c client mode] [-s server mode]\n");
  exit(EXIT_SUCCESS);
}

static int verbose = 0;

int
main(int argc, char **argv)
{
  struct options {
    const char *port;
    const char *host;
  };
  struct options o;
  char opt;

  int socklen, port;  
  struct evconnlistener *listener;  
  static struct sockaddr_storage listen_on_addr;

  memset(&o, 0, sizeof(o));

  if (argc < 2) {
    syntax();
  }

  while ((opt = getopt(argc, argv, "vh:p:uqcs")) != -1) {
    switch (opt) {
    case 'v': ++verbose; break;
    case 'h': o.host = optarg; break;      
    case 'p': o.port = optarg; break;
    default: fprintf(stderr, "Unknow option %c\n", opt); break;      
    }
  }

  if (o.host == NULL) {
    syntax();
  }
  if (o.port == NULL) {
    syntax();
  }

  /* get started */
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
  
  printf(" [INFO Server is up and running: %s:%s]\n", o.host, o.port);
  printf(" [INOF level=%d]\n", verbose);
  
  event_base_dispatch(base);
  
  evconnlistener_free(listener);
  event_base_free(base);

  exit(EXIT_SUCCESS);
}  
