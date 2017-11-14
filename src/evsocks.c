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

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include "internal.h"
#include "evfunc.h"

static const char *
debug_ntoa(uint32_t address)
{
  static char buf[32];
  uint32_t a = ntohl(address);
  evutil_snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
		  (int)(uint8_t)((a>>24)&0xff),
		  (int)(uint8_t)((a>>16)&0xff),
		  (int)(uint8_t)((a>>8 )&0xff),
		  (int)(uint8_t)((a	)&0xff));
  return buf;
}

static void
reader_func(struct bufferevent *bev, void *ctx)
{
  struct bufferevent *partner;
  struct evbuffer *src;  
  size_t len;
  static int times = 0;
  ev_ssize_t evsize;

  partner = ctx;
  src = bufferevent_get_input(bev);
  len = evbuffer_get_length(src); 
  
  if (len != 0) { // got some kind of response
      times++;
      printf("length: %ld, called %d time(s)\n", len, times);
      /* send data depending on the reply */
      unsigned  char buffer[len]; // got output from partner
      evsize = evbuffer_copyout(src, buffer, len);
      
      if (buffer[0] != (uint8_t) SOCKS_VERSION) {
	printf("version is so wrong: %d\n", buffer[0]);
	bufferevent_free(bev);
	return;
      }
      
      switch (buffer[1]) { /* handle socks commands */
      case CONNECT:
      	handle_connect(bev, buffer, evsize);
      	break;
      case BIND:
      	puts("bind");
      	handle_bind(buffer, evsize);
      	break;
      case UDPASSOC:
      	puts("udp associate");
      	handle_udpassoc();
      	break;
      }
  }

  if (!partner) {
    puts("drain");
    evbuffer_drain(src, len);
    return;
  }

  bufferevent_free(bev);
}

static void
handle_addrspec(unsigned char * buffer)
{
  uint8_t spec = buffer[3];
  int buflen;
  unsigned char v4addr[4];
  unsigned char v6addr[16];
  unsigned char *domain;
  unsigned int domlen;
  unsigned char port[2]; /* two bytes for port */
  
  switch(spec) {
  case IPV4:
    buflen = 8;
    memcpy(v4addr, buffer+4, sizeof(unsigned char) * 4);
    printf("[size: %ld]\n", sizeof(v4addr));
    uint32_t a[4] = {v4addr[0], v4addr[1], v4addr[2], v4addr[3]};
    printf("[address: %s]\n", debug_ntoa(a));
    break;
  case IPV6:
    buflen = 12;
    memcpy(v6addr, buffer+4, sizeof(unsigned char) * 16);
    printf("[size: %ld]\n", sizeof(v4addr));
    printf("[first: %d]\n", v4addr[0]);
    printf("[address: %s]\n", v4addr);    
    break;
  case _DOMAINNAME:
    domlen = (int) buffer[4];
    buflen = domlen + 5;
    domain = malloc(sizeof(unsigned char) * domlen);
    memcpy(domain, buffer+4, sizeof(unsigned char) * domlen);
    printf("[domain: %s]\n", domain);
    printf("[length: %d]\n", domlen);
    break;    
  }
  memcpy(port, buffer+buflen, sizeof(unsigned char) * 2); /* allocation for port */
  printf("[PORT: %d %d ]\n", port[0], port[1]);
  // printf("     [PORT: %s, IP: %s %s]\n", port, v4addr, domain);
}

static void
handle_connect(struct bufferevent *bev, unsigned char *buffer, ev_ssize_t evsize)
{
  for (unsigned char i = 0; i < evsize; ++i) {
    printf("%d ", buffer[i]);	
  }
  puts(" ");
  handle_addrspec(buffer);
  send_reply(bev, (uint8_t) SUCCESSED);
}

static void
handle_bind(unsigned char *buffer, ev_ssize_t evsize)
{
  for (unsigned char i = 0; i < evsize; i++) {
    printf("%d ", buffer[i]);	
  }
  puts(" ");
  
  send_reply(NULL, (uint8_t) SUCCESSED);
}

static void
handle_udpassoc(void)
{
  send_reply(NULL, (uint8_t) SUCCESSED);
}

static void
send_reply(struct bufferevent *bev, uint8_t reply)
{
  unsigned char *response;
  // size_t res_size;

  response = get_socks_header();
  // res_size = sizeof(response);
  response[1] = reply;
  puts("send_reply");
}

unsigned char *
get_socks_header(void)
{
  static unsigned char buffer[7]; /* 7 bytes */
  uint8_t socks_version = 5;
  
  socks_version = (uint8_t) 5;
  
  buffer[0] = socks_version;
  buffer[1] = (uint8_t) 0; /* reply */
  buffer[2] = (uint8_t) 0; /* reserved */  
  buffer[3] = (uint8_t) 1; /* address type */
  
  buffer[4] = (uint8_t) 0; /* bind address */
  buffer[5] = (uint8_t) 0; /* bind address */
  buffer[6] = (uint8_t) 0; /* bind address */
  buffer[7] = (uint8_t) 0; /* bind address */
  
  buffer[7] = (uint8_t) 0; /* bind port */
  buffer[8] = (uint8_t) 0; /* bind port */

  return buffer;
}

static void
event_func(struct bufferevent *bev, short what, void *ctx)
{
  struct bufferevent *src = ctx;
  
  if (what & (BEV_EVENT_READING|BEV_EVENT_ERROR)) {
    if (what & BEV_EVENT_ERROR) {
      puts("BEV_EVENT_ERROR");

      if (errno)
	perror("connection error");
    }
    if (what & BEV_EVENT_READING) { /* send reply message to clinets here */
      puts("reading now");
      /* invoke new events here */
    }
    
    if (src) {
      reader_func(bev, ctx);
    }
  }
  
  bufferevent_free(bev);
}

static void
listener_func(struct evconnlistener *listener, evutil_socket_t fd,
	    struct sockaddr *sa, int socklen, void *user_data)
{
  struct event_base *base = user_data;
  struct bufferevent *bev;
  // struct evbuffer *evb;

  char serveraddr[INET_ADDRSTRLEN] = "127.0.0.1";   /* server address */
  unsigned char *header; /* socks5 header this is an estimated size */
  size_t bufsize;

  struct sockaddr_in ss;
  inet_pton(AF_INET, serveraddr, &(ss.sin_addr));
  
  header = get_socks_header();
  bufsize = sizeof(header);
  
  bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
  if (!bev)
    error_exit("bufferevent_socket_new");

  bufferevent_setcb(bev, reader_func, NULL, event_func, NULL);
  if (bufferevent_enable(bev, EV_READ) != 0 )
    error_exit("bufferevet_enable");
  bufferevent_write(bev, header, bufsize);
}

static void
logfn(int is_warn, const char *msg)
{
  if (!is_warn && !verbose)
    return;
  fprintf(stderr, "[%s: %s]\n", is_warn?"WARN":"INFO", msg);
}

static void
syntax(void)
{
  printf("evsocks [--help] [-v] [-h] [--host] [-p] [--port] <listen-on-addr> <port>\n");
  exit(EXIT_SUCCESS);
}

static int verbose = 0;

int
main(int argc, char **argv)
{
  int i, p;
  int has_host = 0;
  int has_port = 0;
  int socklen;
  static struct sockaddr_storage listen_on_addr;
  struct event_base *base;
  struct evconnlistener *listener;  
  
  if (argc < 3)
    syntax();

  for ( i = 1; i < argc; i++) {

    if (strcmp(argv[i], "-v") == 0) {
      printf("verbose+");
      verbose++;
    }

    if (strcmp(argv[i], "-h") == 0) {
      printf("host: %s\n", argv[i+1]);
      has_host = i+1;
    }
    else if (strcmp(argv[i], "--host") == 0) {
      printf("host: %s\n", argv[i+1]);      
      has_host = i+1;
    }
    
    if (strcmp(argv[i], "-p") == 0) {
      printf("port : %s\n", argv[i+1]);
      has_port = i+1;      
    }
    else if (strcmp(argv[i], "--port") == 0)
      has_port = i+1;
    
    if (strcmp(argv[i], "--help") == 0)
      syntax();
  }
  
 /* TODO: let users chagne host */
  fprintf(stderr, "Server is up and running 0.0.0.0:%s\n", argv[has_port]);
  
  memset(&listen_on_addr, 0, sizeof(listen_on_addr));
  socklen = sizeof(listen_on_addr);
  if (evutil_parse_sockaddr_port(argv[has_host],
				 (struct sockaddr*)&listen_on_addr, &socklen)<0) {
    p = atoi(argv[has_port]);
    struct sockaddr_in *sin = (struct sockaddr_in *)&listen_on_addr;
    if (p < 1 || p > 65535)
      syntax();
    (*sin).sin_port = htons(p);
    (*sin).sin_addr.s_addr = htonl(0x7f000001);
    (*sin).sin_family = AF_INET;
    socklen = sizeof(struct sockaddr_in);
  }
  
#ifdef _WIN32
  WSADATA wsa_data;
  WSAStartup(0x0201, &wsa_data);  
#endif

  base = event_base_new();
  if (!base)
    error_exit("event_base_new()");

  listener = evconnlistener_new_bind(base, listener_func, (void *)base,
				     LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
				     (struct sockaddr*)&listen_on_addr,
				     socklen);
  if (!listener)
    error_exit("evconlistner_new_bind");

  /* TODO: Add singal to stop a server */
  event_base_dispatch(base);
  evconnlistener_free(listener);
  event_base_free(base);
  printf("done\n");
  exit(EXIT_SUCCESS);
}
