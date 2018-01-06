/* 
 * slog.h
*/

#ifndef EVS_LOG_H
#define EVS_LOG_H

#include <stdarg.h>

#ifdef __GNUC__
#define S_CHECK_FMT(a,b) __attribute__((format(printf, a, b)))
#define S_NO_RETURN __attribute__((noreturn))
#else
#define S_CHECK_FMT(a,b)
#define S_NO_RETURN
#endif

#define SOCKS_LOG_DEBUG  1
#define SOCKS_LOG_INFO   2
#define SOCKS_LOG_WARN   3
#define SOCKS_LOG_ERROR  4 


void logger_errx(int eval, const char *fmt, ...)S_NO_RETURN;
void logger_err(const char *fmt, ...) S_CHECK_FMT(1,2);
void logger_warn(const char *fmt, ...) S_CHECK_FMT(1,2);
void logger_debug(int v, const char *fmt, ...) S_CHECK_FMT(2, 3);
void logger_info(const char *fmt, ...) S_CHECK_FMT(1,2);
void log_output(int serverity, const char *errstr, const char *fmt, va_list ap) S_CHECK_FMT(3,0);

#endif
