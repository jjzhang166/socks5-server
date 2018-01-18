/* helper functions to handle small tasks */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcip.h>
#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "evs_helper.h"
#include "evs_log.h"
#include "evs_internal.h"


char *
hostcpy(char *dst, char *src, size_t s)
{
  
  while(s--)
    {

      *dst = *src;
      
      if (*dst == 0) return dst;

      *dst ++; *src ++;
      
    }
  
  *dst = 0;
  
  return dst;
}


int
resolve_host(socks_name_t *n)
{  
  struct addrinfo       hints, *res, *p;  
  struct sockaddr_in                sin;
  struct sockaddr_in6              sin6;
  char *host;
  int i;

  host = (char*)malloc(n->len + 1);

  if (host == NULL) {
    free(host);
    logger_err("malloc");
    return -1;
  }

  (void) hostcpy(host, n->host, n->len + 1);

  logger_debug(DEBUG, "host:\"%s\"", host);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host, NULL, &hints, &res) != 0) {
    
    logger_err("host not found %s", host);
        
    free(host);

    return -1;
  }

  free(host);
 
  for (i =0, p =res; p != NULL; p = p->ai_next) {
    switch(p->ai_family) {
    case AF_INET:
    case AF_INET6:
      break;
    default:
      continue;
    }
    i++;
  }

  if (i == 0) { /* no results */
    logger_err("host not found");
    goto failed;
  }

  for (p =res; p != NULL; p = p->ai_next) {
  
    if (p->ai_family != AF_INET)
      continue;

    memcpy(&n->sin, p->ai_addr, p->ai_addrlen);
  
  }

#ifdef SOCKS_HAVE_INET6
  for (p =res; p != NULL; p = p->ai_next) {
    
    if (p->ai_family != AF_INET6)
      continue;
    
    memcpy(&n->sin6, p->ai_addr, p->ai_addrlen);
  }
#endif

  freeaddrinfo(res);  
  return 0;

 failed:
  freeaddrinfo(res);
  return -1;
}
