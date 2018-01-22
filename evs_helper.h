#ifndef HELPER_H
#define HELPER_H

#ifndef SOCKS_HAVE_INET6 // TODO: configure if system has AF_INET6
#define SOCKS_HAVE_INET6 1
#endif

typedef struct {
  char                   *host;
  size_t                   len; // host length
  struct bufferevent      *bev;
  struct sockaddr_in       sin;
#if (SOCKS_HAVE_INET6)  
  struct sockaddr_in6     sin6;
#endif  
} socks_name_t;


int resolve_host(socks_name_t *);
  
#endif
