#ifndef SERVER_H
#define SERVER_H

typedef struct {
  const char *srv_addr;
  short srv_port;
  const char *local_addr;
  short local_port;
  unsigned long timeout;
  const char *password;
  const char *rate_limit;
  const char *worker;  
  int rl;
} srv_conf_t;

void run_srv(srv_conf_t *conf);

#endif
