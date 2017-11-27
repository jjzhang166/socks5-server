/* Implementation of generic handlers */

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
#include <event2/util.h>

#include "internal.h"


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
    ipv4 =   (uint32_t)ip4[0] << 24| /* build address manually ** sigh ** */
             (uint32_t)ip4[1] << 16|
             (uint32_t)ip4[2] << 8 |
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
    break;
  case _DOMAINNAME:
    domlen = buffer[4];
    buflen = domlen + 5;
    (*spec).domain = (unsigned char*)malloc(domlen);
    (*spec).domain = buffer + 5;
    (*spec).sin_family = 3;
    printf("* domain=%s len=%d\n", (*spec).domain, domlen);
    break;
  default:
    fprintf(stderr, "** handle_addrspec.switch Unknown atype\n");
    return NULL;
  }

  memcpy(&pb, buffer+buflen, sizeof(pb));
  port = pb[0]<<8 | pb[1];
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
    
  printf("* in raw ");
  for (i = 0; i < esize; ++i) {
    printf("%d ", buffer[i]);	
  }
  puts(" ");
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

void
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
