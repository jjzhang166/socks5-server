#ifndef HELPER_H
#define HELPER_H

#ifndef SOCKS_HAVE_INET6 // TODO: configure if system has AF_INET6
#define SOCKS_HAVE_INET6 1
#endif

typedef struct socks_name_s {
  // TAILQ_ENTRY (socks_name_s) name_next;

  char                   *host;
  size_t                   len; // host length
  struct bufferevent      *bev;
  struct sockaddr_in       sin;
#if (SOCKS_HAVE_INET6)  
  struct sockaddr_in6     sin6;
#endif  
} socks_name_t;

struct entry_s {
  struct socks_name_s *name;
};

// queue_t * init_queue(queue_t *q, size_t s);
// void queue_insert(queue_t *, queue_t *);
int resolve_host(socks_name_t *);

#endif
