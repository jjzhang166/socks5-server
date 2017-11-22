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
#include "internal.h"

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

#define SOCKS_VERSION 5
 /* address type */
#define IPV4 1
#define IPV6 4
#define _DOMAINNAME 3

 
static struct event_base *base;
static void syntax(void);
static void accept_func(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *a, int slen, void *p);
static void read_func(struct bufferevent *bev, void *ctx);
static void event_func(struct bufferevent *bev, short what, void *ctx);
static void handle_connect(struct bufferevent *bev, unsigned char *buffer, ev_ssize_t esize);

struct addrspec *
handle_addrspec(unsigned char * buffer);
char * get_socks_header(char cmd);

struct addrspec {
  short sin_family;
  unsigned char *domain;
  unsigned long s_addr;
  unsigned char *_s6_addr;
  unsigned short port; /* load with shift operator. otherwise endianness will be messed.. */
};

char *
get_socks_header(char cmd)
{
  static char buffer[9];
  buffer[0] = 5;
  buffer[0] = cmd;   
  buffer[0] = 0;
  buffer[0] = 1;
  
  buffer[0] = 0;
  buffer[0] = 0;
  buffer[0] = 0;
  buffer[0] = 0;
  
  buffer[0] = 0;
  buffer[0] = 0;
  
  return buffer;
}

/* event status */  
/* handling data = 1 */
/* writing data to dst = 2*/
static int status = 0;

static void
read_func(struct bufferevent *bev, void *ctx)
{
  struct evbuffer *src;
  ev_ssize_t esize;
  size_t len;
  
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
      puts("connect");
      evbuffer_drain(src, 3); /* cut 3 bytes */
      handle_connect(bev, buffer, esize);
      break;
    case BIND:
      puts("bind");
      evbuffer_drain(src, 4); /* cut 4 bytes */
      handle_connect(bev, buffer, esize);      
      break;
    case UDPASSOC:
      puts("udp associate");
      // handle_udpassoc();
      break;
    default:
      fprintf(stderr, "[ERROR: read_func.switch cmd not supported]\n");
    }
    /* send an initial reply */
    if (bufferevent_write(bev, payload, 12)<0) /* send out ack reply */
      fprintf(stderr, "read_func.bufferevent_write\n");
    
    /* set statis to reading */
    status = 1;

  } else if (status == 1) {
    printf("esize=%ld; len=%ld\n\n", esize, len);
    for (int i = 0; i < len; i++)
      printf("%c", buffer[i]);
    puts(" ");
    status = 0;
  } else {
    bufferevent_free(bev);
    bufferevent_disable(bev, EV_READ);
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
      bufferevent_free(evbuf);
      bufferevent_disable(evbuf, EV_READ|EV_WRITE);
    }
    
    if (what & BEV_EVENT_READING) {
      puts("[INFO event_func.reading]");
    }
    
    if (what & BEV_EVENT_WRITING) {
      puts("[INFO event_func.WRITING]");
    }
    
    if (evbuf) {
      /* Flush all pending data */
      if (evbuffer_get_length(bufferevent_get_output(evbuf))) {
	printf("what sholud I do ...just disable it...\n");
	
	// bufferevent_setcb(evbuf,
	// 		  NULL, close_on_flush,
	// 		  event_func, NULL);
	bufferevent_disable(evbuf, EV_READ|EV_WRITE);
      } else {
	/* We have nothing left */	
	// bufferevent_free(evbuf);
	return;
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
  struct bufferevent *src, *dst;
  src = bufferevent_socket_new(base, fd,
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
  dst = bufferevent_socket_new(base, fd,
			       BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);  

  /* assert(src && dst); */
  assert(src);
  bufferevent_setcb(src, read_func, NULL, event_func, dst);
  bufferevent_setcb(dst, read_func, NULL, event_func, src);
  bufferevent_enable(src, EV_READ|EV_WRITE);
  bufferevent_enable(dst, EV_READ|EV_WRITE);  
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

void
handle_connect(struct bufferevent *bev, unsigned char *buffer, ev_ssize_t esize)
{
  struct addrspec *spec = malloc(sizeof(struct addrspec));
  // /* ip4 and ip6 are for presentation */
  char ip4[INET_ADDRSTRLEN];
  char ip6[INET6_ADDRSTRLEN];
  
  int i;
  size_t len;
  
  struct evbuffer *src;
  src = bufferevent_get_input(bev);
  len = evbuffer_get_length(src);
    
  printf("\n[INFO: ");
  for (i = 0; i < esize; ++i) {
    printf("%d ", buffer[i]);	
  }
  puts("]");

  if (esize <=4 )
    return; /* nothing to get from this buffer */
  
  spec = handle_addrspec(buffer);
  if ((spec == NULL)) {
    return;
  }

  /* going to present address */
  switch ((*spec).sin_family) {
  case AF_INET:
    if (!((inet_ntop(AF_INET, &((*spec).s_addr), ip4, INET_ADDRSTRLEN)) == NULL)) {
      printf("[INFO: v4=%s:%d]\n", ip4, (*spec).port);
    }
    break;
  case AF_INET6:
    if (!((inet_ntop(AF_INET6, &((*spec)._s6_addr), ip6, INET6_ADDRSTRLEN)) == NULL)) {
      printf("[INFO: v6=%s:%d]\n", ip6, (*spec).port);
    }
    break;
  case 3:
    printf("[INFO: domain=%s:%d]\n", (*spec).domain, (*spec).port);
    break;
  default:
    fprintf(stderr, "[ERROR: Unknow family]\n");
    break;
  }
  /* drain a read buffer */
  evbuffer_drain(src, len);
}

static void
syntax(void)
{
  printf("evsocks [--help] [-v] [-h] [-p]\n");
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

  while ((opt = getopt(argc, argv, "vh:p:")) != -1) {
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
