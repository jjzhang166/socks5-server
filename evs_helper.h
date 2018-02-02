#ifndef HELPER_H
#define HELPER_H

#ifndef SOCKS_HAVE_INET6 // TODO: configure if system has AF_INET6
#define SOCKS_HAVE_INET6 1
#endif

#include "evs_internal.h"
#include <sys/socket.h>


typedef struct {
  struct sockaddr *sockaddr;
  socklen_t         socklen;    
} socks_addr_t;


typedef struct socks_name_s {
  char                   *host;
  size_t                   len; // host length
  u16                     port;
  socks_addr_t          *addrs;
  int                   family;
  struct bufferevent      *bev;
  struct sockaddr_in      *sin;

#if (SOCKS_HAVE_INET6)  
  struct sockaddr_in6    sin6;
#endif  

} socks_name_t;


struct entry_s {
  struct socks_name_s *name;
};


int resolve_host(socks_name_t *);

#endif
