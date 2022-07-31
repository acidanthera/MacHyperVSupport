//
//  hvdebug.h
//  Hyper-V userspace debugging support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef hvdebug_h
#define hvdebug_h

#include <stdio.h>

#if DEBUG
#define HVDeclareLogFunctionsUser(appName) \
  static inline void HVLOG_PRINT(const char *func, FILE *file, const char *str, ...) { \
    char tmp[256]; \
    tmp[0] = '\0'; \
    va_list args; \
    va_start(args, str); \
    vsnprintf(tmp, sizeof (tmp), str, args); \
    fprintf(file, "%s: %s(): %s\n", appName, func, tmp); \
    va_end(args); \
  }

#define HVDBGLOG(file, str, ...) HVLOG_PRINT(__FUNCTION__, file, str, ## __VA_ARGS__)
#define HVSYSLOG(file, str, ...) HVLOG_PRINT(__FUNCTION__, file, str, ## __VA_ARGS__)

#else
#define HVDeclareLogFunctionsUser(appName) \
  static inline void HVLOG_PRINT(FILE *file, const char *str, ...) { \
    char tmp[256]; \
    tmp[0] = '\0'; \
    va_list args; \
    va_start(args, str); \
    vsnprintf(tmp, sizeof (tmp), str, args); \
    fprintf(file, "%s: %s\n", appName, tmp); \
    va_end(args); \
  }

#define HVDBGLOG(file, str, ...) {}
#define HVSYSLOG(file, str, ...) HVLOG_PRINT(file, str, ## __VA_ARGS__)
#endif

#endif
