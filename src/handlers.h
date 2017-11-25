/* copyright (C) Xun, 2017 */

#ifndef HANDLERS_H
#define HANDLERS_H


#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcip.h>
#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <event2/bufferevent.h>

#include "internal.h"

struct addrspec * handler_addrspec(unsigned char * buffer);

struct addrspec * handle_connect(struct bufferevent *bev, unsigned char *buffer, ev_ssize_t evsize);

static void debug_addr(struct addrspec *spec);

#endif
