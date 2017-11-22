/* copyright (C) Xun, 2017 */

#include <stdarg.h>
#include "internal.h"

void
error_exit(const char *format, ...)
{
  va_list args;

  va_start(args, format);

#define ERROR_BUF 500
  char *s;
  char buf[ERROR_BUF], foruser[ERROR_BUF],  errtext[ERROR_BUF];
  vsnprintf(foruser, ERROR_BUF, format, args);
  snprintf(errtext, ERROR_BUF, ":");
  snprintf(buf, ERROR_BUF, "ERROR %s %s\n", errtext, foruser);

  fputs(buf, stderr);
  fflush(stderr);
  
  s = getenv("EF_DUMPCORE");
  if (s != NULL && *s != '\0')
    abort();
  else _exit(EXIT_FAILURE);
}
