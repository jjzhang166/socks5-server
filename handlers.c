/* 
 * handler.c
 *
 * Use of this source code is governed by a
 * license that can be found in the LICENSE file.
 *
 * Implementation of generic socks handlers 
 *
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
handle_addrspec(u8 *buffer)
{
  struct addrspec *spec;
  // struct addrinfo hints, *res, *p; /* for getaddrinfo */

  int buflen, domlen;

  char b[128]; /* 128 bits for addresses */
  u32 ipv4; /* 32 bits for IPv4 */
  u32 s_addr; /* 32 bits for IPv4 */
  u16 port; /* short for port  */
  u8 atype = buffer[3];
  u8 ip4[4];
  u8 pb[2]; /* 2 bytes for port */

  spec = malloc(sizeof(struct addrspec));

  if (spec == NULL) {
    logger_err("handle_addrspec.malloc");
    return NULL;
  }
  
  switch (atype) {
  case IPV4:
    buflen = 8;
    memcpy(ip4, buffer+4, 4);
    ipv4 =   (u32)ip4[0] << 24| /* build address manually ** sigh ** */
             (u32)ip4[1] << 16|
             (u32)ip4[2] << 8 |
             (u32)ip4[3];
    s_addr = htonl(ipv4);
    spec->s_addr = s_addr;
    spec->family = AF_INET;
    break;
  case IPV6:
    buflen = 20;    
    spec->family = AF_INET6;
    memcpy(spec->ipv6_addr, buffer+4, 16); /* 4 steps for jumping to 16 bytes address */
    if(!(evutil_inet_ntop(AF_INET6, &(spec->ipv6_addr), b, sizeof(b)))) {
      logger_err("inet_ntop(AF_INET6..");
      free(spec);
      return NULL;
    }
    if (evutil_inet_pton(AF_INET6, b, spec->ipv6_addr)<0) {
      logger_err("inet_pton(AF_INET6..");
      free(spec);
      return NULL;
    }
    break;
  case _DOMAINNAME:
    /* TODO:
     *  look up domains asynchronically
     *  most cases, getaddrinfo is stuck here
     *  and eventually time-out will occur.
     */
    domlen = buffer[4];
    buflen = domlen+5;

    spec->domain = calloc(domlen, sizeof(const char));
    spec->family = AF_INET;
    memcpy(spec->domain, buffer+5, domlen);
    
    if (resolve_host(spec->domain, domlen, spec)<0)
      return NULL;
        
    break;
  default:
    logger_err("handle_addrspec.switch Unknown atype");
    free(spec);
    return NULL;
  }
  memcpy(&pb, buffer+buflen, sizeof(pb));
  port = pb[0]<<8 | pb[1];
  spec->port = port;
  return spec;
}

int
resolve_host(char *domain, int len, struct addrspec *spec)
{
  struct addrinfo hints, *res, *p;
  struct sockaddr_in   sin;
  struct sockaddr_in6 sin6;
  char b4[SOCKS_INET_ADDRSTRLEN];
  char b6[SOCKS_INET6_ADDRSTRLEN];
  int i;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  /* try to check domain.. */
  if (!strchr(domain, '.')) {
    return -1;
  }
  
  if (getaddrinfo((char *) domain, NULL, &hints, &res)<0) {
    logger_err("getaddrinfo host not(%s) found", domain);
    return -1;
  }
  
  for (i = 0, p = res; p != NULL; p = p->ai_next) {
    switch (p->ai_family) {
    case AF_INET:
    case AF_INET6:
      break;
    default:
      continue;
    }
    i++;
  }
  
  if (i == 0) { /* no results */
    logger_err("host(%s) not found", domain);
    return -1;
  }
  
  /* start with AF_INET */
  for (p = res; p != NULL; p = p->ai_next) {
    if (p->ai_family != AF_INET)
      continue;
    memcpy(&sin, p->ai_addr, p->ai_addrlen);
    if (evutil_inet_ntop(AF_INET, (struct sockaddr*)&(sin.sin_addr),
			 b4, SOCKS_INET_ADDRSTRLEN) == NULL)
      return -1;
    
    if (spec != NULL)
      spec->s_addr = sin.sin_addr.s_addr;
    
    logger_info("resolve host->%s", b4);
  }

  /* then, AF_INET6 */
  for (p = res; p != NULL; p = p->ai_next) {
    if (p->ai_family != AF_INET6)
      continue;    
    memcpy(&sin6, p->ai_addr, p->ai_addrlen);    
    if (evutil_inet_ntop(AF_INET6, (struct sockaddr*)&(sin6.sin6_addr),
			 b6, SOCKS_INET6_ADDRSTRLEN) == NULL)
      return -1;

    logger_info("resolve host->%s", b6);    
  }
  
  freeaddrinfo(res);
  return 1;
}

u8 *
cpystrn(u8 *dst, u8 *src, size_t s)
{
  if (s == 0)
    return dst;
  while (--s) {
    *dst = *src;
    if (*dst == '\0')
      return dst;
    dst++;
    src++;
  }
  *dst = '\0';
  return dst;
}

void
debug_addr(struct addrspec *spec)
{
  /* ip4 and ip6 are for presentation */
  char b4[SOCKS_INET_ADDRSTRLEN];
  char b6[SOCKS_INET6_ADDRSTRLEN];  
  
  if (!spec) {
    return;
  }
  /* going to present address */
  switch (spec->family) {
  case AF_INET:
    if (evutil_inet_ntop(AF_INET, &(spec->s_addr), b4, SOCKS_INET_ADDRSTRLEN)) {
      logger_info("ip4=%s:%d", b4, spec->port);
    }
    break;
  case AF_INET6:
    if (evutil_inet_ntop(AF_INET6, &(spec->ipv6_addr), b6, SOCKS_INET6_ADDRSTRLEN)) {
      logger_info("ip6=%s:%d", b6, spec->port);
    }
    break;
  default:
    logger_err("Unknow addr family");
    break;
  }
}
