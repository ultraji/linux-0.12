#ifndef _LOG_PRINT_H
#define _LOG_PRINT_H

#define LOG_INFO      0
#define LOG_DEBUG     1
#define LOG_WARN      2

#define DEBUG

#ifndef DEBUG
#define DEBUG_PRINT(format, ...)
#else
#define DEBUG_PRINT(format, ...) log_print(LOG_DEBUG, "[%s:%d][%s] " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif

#endif
