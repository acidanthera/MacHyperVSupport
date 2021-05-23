//
//  HyperV.hpp
//  MacHyperVServices
//
//  Created by John Davis on 5/5/21.
//

#ifndef HyperV_hpp
#define HyperV_hpp

#include <IOKit/IOLib.h>

#define kHyperVStatusSuccess    0
#define kHyperVStatusFail       0x80004005

#define BIT(a)                  (1 << (a))

#define HV_PAGEALIGN(a)         (((a) + (PAGE_SIZE - 1)) &~ (PAGE_SIZE - 1))

#define kHyperVHypercallRetryCount  20



#define LOG_PRINT(className, str, ...) logPrint(className, __FUNCTION__, str, ## __VA_ARGS__)

#if DEBUG
//
// Debug logging function.
//
inline void logPrint(const char *className, const char *funcName, const char *format, ...) {
  char tmp[256];
  tmp[0] = '\0';
  va_list va;
  va_start(va, format);
  vsnprintf(tmp, sizeof (tmp), format, va);
  va_end(va);
  
  IOLog("%s::%s(): %s\n", className, funcName, tmp);
}

#define DBGLOG_PRINT(className, str, ...) logPrint(className, __FUNCTION__, str, ## __VA_ARGS__)
#define SYSLOG_PRINT(className, str, ...) logPrint(className, __FUNCTION__, str, ## __VA_ARGS__)
#else

//
// Release print function.
//
inline void logPrint(const char *className, const char *format, ...) {
  char tmp[256];
  tmp[0] = '\0';
  va_list va;
  va_start(va, format);
  vsnprintf(tmp, sizeof (tmp), format, va);
  va_end(va);
  
  IOLog("%s: %s\n", className, tmp);
}

#define DBGLOG_PRINT(className, str, ...) {}
#define SYSLOG_PRINT(className, str, ...) logPrint(className, str, ## __VA_ARGS__)
#endif


template <class T, size_t N>
constexpr size_t ARRAY_SIZE(const T (&array)[N]) {
  return N;
}

#define kHyperVMessageDataSizeMax     240
#define kHyperVMessageSize            256

typedef union {
  UInt8 value;
  struct {
    UInt8 messagePending : 1;
    UInt8 reserved : 7;
  } __attribute__((packed));
} HyperVMessageFlags;

typedef union {
  UInt32 value;
  struct {
    UInt32 id : 24;
    UInt32 reserved : 8;
  } __attribute__((packed));
} HyperVPortId;

typedef struct __attribute__((packed)) {
  UInt32              type;
  UInt8               size;
  HyperVMessageFlags  flags;
  UInt16              reserved;
  union {
    UInt64            sender;
    HyperVPortId      portId;
  };
  UInt8               data[kHyperVMessageDataSizeMax];
} HyperVMessage;

//
// GPADL range
//
typedef struct {
  UInt32  byteCount;
  UInt32  byteOffset;
  UInt64  pfnArray[];
} HyperVGPARange;

#define HyperVEventFlagsByteCount 256

//
// Event flags.
//
typedef struct __attribute__((packed)) {
  UInt8 flags[HyperVEventFlagsByteCount];
} HyperVEventFlags;

typedef struct __attribute__((packed)) {
  UInt32 connectionId;
  UInt16 eventFlagsOffset;
  UInt16 reserved;
} HyperVMonitorNotificationParameter;

#endif
