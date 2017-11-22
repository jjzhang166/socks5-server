/* copyright (C) Xun, 2017 */

#ifndef INTERNAL_H
#define INTERNAL_H

#include <stdio.h>
#include <stdlib.h> /* EXIT_SUCCESS and EXIT_FAILURE */
#include <unistd.h>
#include <errno.h>
#include <string.h>

#ifdef __GNUC__

/* This macro stops gcc -Wall complaining */

#define NORETURN __attribute__ ((__noreturn__))
#else
#define NORETURN
#endif

void error_exit(const char *format, ...) NORETURN;

/* socks version  */
#define SOCKS_VERSION 5

/* socks commands */
#define CONNECT 1
#define BIND 2
#define UDPASSOC 3

 /* server replies */
#define SUCCESSED                  0
#define GENERAL_FAILURE            1
#define NOT_ALLOWED                2
#define NETWORK_UNREACHABLE        3
#define HOST_UNREACHABLE           4
#define REFUSED                    5
#define TTL_EXPIRED                6
#define NOT_SUPPORTED              7
#define ADDRESS_TYPE_NOT_SUPPORTED 8
#define  UNASSIGNED                9

 /* address type */
#define IPV4        1
#define IPV6        4
#define _DOMAINNAME 3

#define SOCKS_VERSION 5

#define SREAD     1
#define SWRTIE    2
#define SWAIT     3
#define SHANG     4
#define SDESTORY  5


struct addrspec {
  short sin_family;
  unsigned char *domain;
  unsigned long s_addr;
  unsigned char *_s6_addr;
  unsigned short port;
};

static int status;
 
static struct event_base *base;

static void syntax(void);

static void accept_func(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *a, int slen, void *p);

static void read_func(struct bufferevent *bev, void *ctx);

static void event_func(struct bufferevent *bev, short what, void *ctx);

static void handle_connect(struct bufferevent *bev, unsigned char *buffer, ev_ssize_t esize);

struct addrspec * handle_addrspec(unsigned char * buffer);

char * get_socks_header(char cmd);

#endif
