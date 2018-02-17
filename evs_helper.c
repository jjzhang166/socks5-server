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
#include "evs_lru.h"

static char * hostcpy(char *dst, char *src, size_t s);

int
resolve_host(socks_name_t *n, lru_node_t **node)
{  
  struct addrinfo hints, *res, *p;  
  struct sockaddr_in *sin;
  struct sockaddr_in6 *sin6;
  u8 buf[128];
  char *host;
  int i;

  host = (char*)malloc(n->len + 1);

  if (host == NULL) {
    log_err("malloc");
    return -1;
  }
  
  (void) hostcpy(host, n->host, n->len);
  
  log_debug(DEBUG, "resolve: %s", host);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  lru_node_t *cached = lru_get_node(node, (void*)host, (lru_cmp_func*)strcmp);
  /* Check if we already have the key entropy */
  if (cached != NULL) {
    log_info("cached=%s", cached->key);
  }
  
  if (getaddrinfo((char *)host, NULL, &hints, &res) != 0) {
    log_err("host not found");
    free(host);
    return -1;
  }
 
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
    log_err("host not found");
    goto failed;
  }

  for (p =res; p !=NULL; p =p->ai_next) {

    if (p->ai_family != AF_INET)
      continue;

    memcpy(&n->sin, p->ai_addr, p->ai_addrlen);
    break;
  }

  if (lru_insert_left(node, host, (void*)n, sizeof(n)) != false) {
    log_warn("lru_insert");
  }
  
  freeaddrinfo(res);  
  return 0;

 failed:
  freeaddrinfo(res);
  return -1;
}

static char *
hostcpy(char *dst, char *src, size_t s)
{
  
  while(s--)
    {

      *dst = *src;
      
      if (*dst == '\0') return dst;

      *dst ++; *src ++;
      
    }
  
  *dst = '\0';
  
  return dst;
}
