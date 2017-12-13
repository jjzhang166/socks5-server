/* 
 * handler.c
 *
 * Use of this source code is governed by a
 * license that can be found in the LICENSE file.
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
handle_addrspec(u8 *buffer)
{
  struct addrspec *spec;
  struct addrinfo hints, *res, *p; /* for getaddrinfo */
  
  u8 atype = buffer[3];
  u8 ip4[4];
  u8 pb[2]; /* 2 bytes for port */
  
  u16 port; /* short for port  */

  u32 ipv4; /* 32 bits for IPv4 */
  u32 s_addr; /* 32 bits for IPv4 */
  
  // char ipv6[INET6_ADDRSTRLEN]; /* 128 bits for IPv6 */
  char b[128];
  
  int buflen, domlen;

  spec = malloc(sizeof(struct addrspec));

  switch (atype) {
  case IPV4:
    buflen = 8;
    memcpy(ip4, buffer+4, 4);
    ipv4 =   (u32)ip4[0] << 24| /* build address manually ** sigh ** */
             (u32)ip4[1] << 16|
             (u32)ip4[2] << 8 |
             (u32)ip4[3];
    s_addr = htonl(ipv4);
    (*spec).s_addr = s_addr;
    (*spec).family = AF_INET;
    break;
  case IPV6:
    buflen = 20;    
    (*spec).family = AF_INET6;
    memcpy((*spec).ipv6_addr, buffer+4, 16); /* 4 steps for jumping to 16 bytes address */

    if(!(evutil_inet_ntop(AF_INET6, &((*spec).ipv6_addr), b, sizeof(b)))) {
      logger_err("inet_ntop(AF_INET6..");
      return NULL;
    }

    if (evutil_inet_pton(AF_INET6, b, (*spec).ipv6_addr)<0) {
      logger_err("inet_pton(AF_INET6..");      
      return NULL;
    }    
    logger_info("ip6=%s", b); 
    break;
  case _DOMAINNAME:
    /* TODO:
     *  look up domains asynchronically
     *  most cases, getaddrinfo is stuck here
     *  and eventually time-out will occur.
     */
    domlen = buffer[4];
    buflen = domlen+5;
    
    (*spec).domain = calloc(domlen, sizeof(const char));
    memcpy((*spec).domain, buffer+5, domlen);        
    (*spec).family = 3;
    
    logger_info("doamin:%s", (*spec).domain);
	
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo((*spec).domain, NULL, &hints, &res)<0) {
      perror("getaddrinfo");
      return NULL;
    }

    for (p =res; p !=NULL; p =(*p).ai_next) {
      if ((*p).ai_family == AF_INET) {
    	struct sockaddr_in* v4 = (struct sockaddr_in*)(*p).ai_addr;
    	(*spec).s_addr = (*v4).sin_addr.s_addr;
    	(*spec).family = AF_INET; /* force to use IPv4.. */
      }
    }

    freeaddrinfo(res);
    logger_debug(verbose, "freed addrinfo");
    break;
  default:
    logger_err("handle_addrspec.switch Unknown atype");
    return NULL;
  }
  memcpy(&pb, buffer+buflen, sizeof(pb));
  port = pb[0]<<8 | pb[1];
  (*spec).port = port;
  return spec;
}

void
debug_addr(struct addrspec *spec)
{
  /* ip4 and ip6 are for presentation */
  char b[128];
  
  if (spec == NULL) {
    return;
  }
  logger_info("debug_addr");
  /* going to present address */
  switch ((*spec).family) {
  case AF_INET:
    if (evutil_inet_ntop(AF_INET, &((*spec).s_addr), b, sizeof(b))) {
      logger_info("to ip4=%s:%d", b, (*spec).port);
    }
    break;
  case AF_INET6:
    if (evutil_inet_ntop(AF_INET6, &((*spec).ipv6_addr), b, sizeof(b))) {
      logger_info("to ip6=%s:%d", b, (*spec).port);
    }
    break;
  case 3:
    logger_debug(verbose, "to domain=%s:%d", (*spec).domain, (*spec).port);
    break;
  default:
    logger_err("Unknow addr family");
    break;
  }
}
