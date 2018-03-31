/* 
 * Watch out - this is a work in progress!
 *
 * main.c
 *
 * Use of this source code is governed by a
 * license that can be found in the LICENSE file.
 *
 * Simple proxy server with Libevent 
 * 
 */

#include "evs_log.h"
#include "evs_internal.h"
#include "evs_server.h"

#define BIND_HOST "0.0.0.0"
#define LOCAL_HOST "127.0.0.1"
#define BIND_PORT 1080
#define PASSWORD "too_lame_to_set_password"
#define TIMEOUT 300
#define RUN_LOCAL 0

static void usage(void);
static void parse_opt(srv_conf_t*, int, char**);

int
main(int c, char **v)
{
  srv_conf_t conf;

  memset(&conf, 0, sizeof(conf));
  conf.srv_addr = BIND_HOST;
  conf.srv_port = BIND_PORT;
  conf.local_addr = LOCAL_HOST;
  conf.local_port = BIND_PORT;
  conf.password = PASSWORD;
  conf.timeout = TIMEOUT;
  conf.rl = RUN_LOCAL; /* run as non-local mode default */
  
  parse_opt(&conf, c, v);
  
  if (conf.rl) {
    log_info("client server %s:%d", conf.local_addr, conf.local_port);
  } else {
    log_info("%s:%d", conf.srv_addr, conf.srv_port);
    log_debug(DEBUG, "timeout=%ld", conf.timeout);
  }
  (void)run_srv(&conf);
}

static
void parse_opt(srv_conf_t *conf, int c, char **v)
{
  int cc = 0;
  int port;
  
  while (cc != -1) {
    cc = getopt(c, v, "lhs:p:u:j:k:w:r:t:");
    
    switch(cc) {
    case 's':
      conf->srv_addr = optarg; break;
    case 'p':
      port = atoi(optarg);
      if (port < 1 || port > 65535)
	usage();
      conf->srv_port = port; break;      
    case 'u':
      conf->local_addr = optarg; break;      
    case 'j':
      port = atoi(optarg);
      if (port < 1 || port > 65535)
	usage();      
      conf->local_port = port; break;      
    case 'k':
      conf->password = optarg; break;      
    case 'w':
      conf->worker = optarg; break;      
    case 'r':
      conf->rate_limit = optarg; break;
    case 'l':
      conf->rl = 1; break;
    case 't':
      // TODO: check if timeout is unsigned long 
      conf->timeout = atoi(optarg); break;
    case 'h':
      usage();
    }
  }
}

static
void usage() {
  printf("Usage: esocks [-s <address>] [-h] [-p port] [-k password] [-t timeout]\n"
	 "\n"
	 "options:\n"
	 "  -s bind to this address\n"
	 "  -p port\n"
	 "  -u local address\n"
	 "  -j local port\n"
	 "  -k password\n"
	 "  -w workern\n"
	 "  -t timeout default 300\n"
	 "  -r limit rate in bytes\n");
  exit(1);
}
