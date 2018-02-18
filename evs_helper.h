#ifndef HELPER_H
#define HELPER_H

#ifndef SOCKS_HAVE_INET6 // TODO: configure if system has AF_INET6
#define SOCKS_HAVE_INET6 1
#endif

#include "evs_internal.h"
#include "evs_lru.h"
#include <sys/socket.h>


typedef struct {
  struct sockaddr *sockaddr;
  socklen_t         socklen;    
} socks_addr_t;

typedef struct socks_name_s {
  u8 hlen; // host length
  u16 port;  
  int family;
  int naddr;
  char *host;  
  socks_addr_t *addrs;
  struct bufferevent *bev;
  struct sockaddr *sa;
} socks_name_t;

struct entry_s {
  struct socks_name_s *name;
};

int resolve_host(socks_name_t *, lru_node_t **);
#endif
