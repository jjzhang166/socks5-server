/* 
 * handler.c
 * Copyright (c) 2017 Xun
 *
 * Implementation of generic socks handlers 
*/

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
#include "slog.h"

struct addrspec *
handle_addrspec(unsigned char * buffer)
{
  struct addrspec *spec;
  unsigned char atype = buffer[3];
  int buflen, domlen;  
  unsigned long s_addr;
  unsigned char ip4[4];
  uint32_t ipv4;
  char ipv6[INET6_ADDRSTRLEN];
  unsigned char pb[2]; /* 2 bytes for port */
  unsigned short port;

  struct addrinfo hints, *res, *p; /* for getaddrinfo */
  char *dstr;

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
    (*spec).sin_family = AF_INET6;
    memcpy((*spec)._s6_addr, buffer+4, 16); /* 4 steps for jumping to 16 bytes address */
    if (!(inet_ntop(AF_INET6, &((*spec)._s6_addr), ipv6, INET6_ADDRSTRLEN))) {
      logger_err("inet_ntop(AF_INET6..");
    }

    logger_debug("v6 %s", ipv6);
    
    break;
  case _DOMAINNAME:
    domlen = buffer[4];
    buflen = domlen + 5;
    (*spec).domain = (unsigned char*)malloc(domlen);
    (*spec).domain = buffer + 5;
    (*spec).sin_family = 3;

    dstr = malloc(domlen);
    sprintf(dstr, "%s", (*spec).domain);
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; /* force to use IPV4 */
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(dstr, NULL, &hints, &res)!=0) {
      logger_err("getaddrinfo");
      return NULL;
    }
    for (p = res; p != NULL; p = (*p).ai_next) {
      if ((*p).ai_family == AF_INET) {
	struct sockaddr_in* v4 = (struct sockaddr_in*)(*p).ai_addr;
	(*spec).s_addr = (*v4).sin_addr.s_addr;
	(*spec).sin_family = AF_INET;
      }
    }
    break;
  default:
    logger_err("handle_addrspec.switch Unknown atype");
    return NULL;
  }

  memcpy(&pb, buffer+buflen, sizeof(pb));
  port = pb[0]<<8 | pb[1];
  (*spec).port = port;
  
  debug_addr(spec);
  
  return spec;
}

struct addrspec *
handle_connect(struct bufferevent *bev, unsigned char *buffer, ev_ssize_t esize)
{
  struct addrspec *spec = malloc(sizeof(struct addrspec));
  size_t len;
  struct evbuffer *src;
  
  src = bufferevent_get_input(bev);
  len = evbuffer_get_length(src);

  if (esize <=4 )
    return NULL; /* nothing to get from this buffer */
  
  spec = handle_addrspec(buffer);

  /* drain a read buffer */
  evbuffer_drain(src, len);

  return spec;
}

/* void 
 * handle_udp_associate()
 *  { 
} 
*/

void
debug_addr(struct addrspec *spec)
{
  /* ip4 and ip6 are for presentation */
  char ip4[INET_ADDRSTRLEN];
  char ip6[INET6_ADDRSTRLEN];
  
  if (spec == NULL) {
    return;
  }
  
  /* going to present address */
  switch ((*spec).sin_family) {
  case AF_INET:
    if (!((inet_ntop(AF_INET, &((*spec).s_addr), ip4, INET_ADDRSTRLEN)) == NULL)) {
      logger_info("to v4=%s:%d", ip4, (*spec).port);
    }
    break;
  case AF_INET6:
    if (!((inet_ntop(AF_INET6, &((*spec)._s6_addr), ip6, INET6_ADDRSTRLEN)) == NULL)) {
      logger_debug("to v6=%s:%d", ip6, (*spec).port);
    }
    break;
  case 3:
    logger_debug("to domain=%s:%d", (*spec).domain, (*spec).port);
    break;
  default:
    logger_debug("Unknow addr family");
    break;
  }
}
