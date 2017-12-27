
#ifndef TINY_TEST_H
#define TINY_TEST_H

#include <stdarg.h>

#ifdef __GNUC__
#define TEST_FMT(a,b) __attribute__((format(printf, a, b)))
#else
#define TEST_FMT(a,b)
#endif


#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

void test_ok(const char *fmt, ...) TEST_FMT(1,2);
void test_failed(const char *fmt, ...) TEST_FMT(1,2);
void announce(const char *name, ...) TEST_FMT(1,2);
void announce_(int ok_or_fail, const char *msg, va_list ap) TEST_FMT(2, 0);


struct regress_host {
  u8 *domain;
  u8 port[2];
  int len;
};

struct regress_bytes_data {
  const char *name;
  u8 buffer[256];
};

#endif
