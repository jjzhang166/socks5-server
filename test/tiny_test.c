#include "../evs_internal.h"
#include "tiny_test.h"

void
announce(const char *name, ...)
{
  (void)fprintf(stderr, "%s\n", name);
}

void announce_(int ok_or_fail, const char *msg, va_list ap)
{
  size_t len;  
  char buf[1024];
  char *ok = "OK";
  char *failed = "FAILED";
  char *status;
  
  evutil_vsnprintf(buf, sizeof(buf), msg, ap);

  if (ok_or_fail)
    status = strdup(ok);
  else
    status = strdup(failed);
  
  (void)fprintf(stderr, "[%s] %s\n", status, buf);
}

void
test_ok(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  va_end(ap);
  
  announce_(1, fmt, ap);
}

void
test_failed(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  va_end(ap);
  
  announce_(0, fmt, ap);
  exit(1);
}
