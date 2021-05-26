//
//  HyperV.hpp
//  Hyper-V register and structures header
//
//  Copyright © 2021 Goldfish64. All rights reserved.
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

//
// Pages are assumed to be 4KB
//
#if PAGE_SHIFT != 12
#error Invalid page shift
#endif

//
// Hyper-V CPUID leaves
//
#define kHyperVCpuidMaxLeaf           0x40000000

#define kHyperVCpuidLeafInterface     0x40000001
#define kHyperVCpuidLeafInterfaceSig  0x31237648 // HV#1

#define kHyperVCpuidLeafIdentity      0x40000002

#define kHyperVCpuidLeafFeatures      0x40000003
#define CPUPM_HV_CSTATE_MASK    0x000f  /* deepest C-state */
#define CPUPM_HV_C3_HPET    0x0010  /* C3 requires HPET */
#define CPUPM_HV_CSTATE(f)    ((f) & CPUPM_HV_CSTATE_MASK)
/* EDX: features3 */
#define CPUID3_HV_MWAIT      0x0001  /* MWAIT */
#define CPUID3_HV_XMM_HYPERCALL    0x0010  /* Hypercall input through
             * XMM regs */
#define CPUID3_HV_GUEST_IDLE    0x0020  /* guest idle */
#define CPUID3_HV_NUMA      0x0080  /* NUMA distance query */
#define CPUID3_HV_TIME_FREQ    0x0100  /* timer frequency query
             * (TSC, LAPIC) */
#define CPUID3_HV_MSR_CRASH    0x0400  /* MSRs for guest crash */

#define kHyperVCpuidLeafRecommends    0x40000004
#define kHyperVCpuidLeafLimits        0x40000005
#define kHyperVCpuidLeafHwFeatures    0x40000006

//
// Hyper-V CPUID feature support
//
#define MSR_HV_TIME_REF_COUNT    0x40000020

#define kHyperVCpuidMsrTimeRefCnt      0x0002
#define kHyperVCpuidMsrSynIC           0x0004
#define kHyperVCpuidMsrSynTimer        0x0008
#define kHyperVCpuidMsrAPIC            0x0010
#define kHyperVCpuidMsrHypercall       0x0020
#define kHyperVCpuidMsrVPIndex         0x0040
#define kHyperVCpuidMsrReferenceTsc    0x0200
#define kHyperVCpuidMsrGuestIdle       0x0400

#ifndef NANOSEC
#define NANOSEC        1000000000ULL
#endif
#define HYPERV_TIMER_NS_FACTOR    100ULL
#define HYPERV_TIMER_FREQ    (NANOSEC / HYPERV_TIMER_NS_FACTOR)

//
// Hyper-V MSRs
//
#define kHyperVMsrGuestID                     0x40000000
#define kHyperVMsrGuestIDBuildMask              0xFFFFULL
#define kHyperVMsrGuestIDServicePackMask        0x0000000000FF0000ULL
#define kHyperVMsrGuestIDServicePackShift       16
#define kHyperVMsrGuestIDMinorVersionMask       0x00000000FF000000ULL
#define kHyperVMsrGuestIDMinorVersionShift      24
#define kHyperVMsrGuestIDMajorVersionMask       0x000000FF00000000ULL
#define kHyperVMsrGuestIDMajorVersionShift      32
#define kHyperVMsrGuestIDSystemIDMask           0x00FF000000000000ULL
#define kHyperVMsrGuestIDSystemIDShift          48
#define kHyperVMsrGuestIDSystemTypeMask         0x7F00000000000000ULL
#define kHyperVMsrGuestIDSystemTypeShift        56
#define kHyperVMsrGuestIDOpenSource             0x8000000000000000ULL

#define kHyperVMsrHypercall                     0x40000001
#define kHyperVMsrHypercallEnable               0x0001ULL
#define kHyperVMsrHypercallRsvdMask             0x0FFEULL
#define kHyperVMsrHypercallPageShift            PAGE_SHIFT

#define kHyperVMsrVPIndex                       0x40000002

#define kHyperVMsrReferenceTsc                  0x40000021
#define kHyperVMsrReferenceTscEnable            0x0001ULL
#define kHyperVMsrReferenceTscRsvdMask          0x0FFEULL
#define kHyperVMsrReferenceTscPageShift         PAGE_SHIFT

#define kHyperVMsrSyncICControl                 0x40000080
#define kHyperVMsrSyncICControlEnable           0x0001ULL
#define kHyperVMsrSyncICControlRsvdMask         0xFFFFFFFFFFFFFFFEULL

#define kHyperVMsrSiefp                         0x40000082
#define kHyperVMsrSiefpEnable                   0x0001ULL
#define kHyperVMsrSiefpRsvdMask                 0x0FFEULL
#define kHyperVMsrSiefpPageShift                PAGE_SHIFT

#define kHyperVMsrSimp                          0x40000083
#define kHyperVMsrSimpEnable                    0x0001ULL
#define kHyperVMsrSimpRsvdMask                  0x0FFEULL
#define kHyperVMsrSimpPageShift                 PAGE_SHIFT

#define kHyperVMsrEom                           0x40000084

#define kHyperVMsrSInt0                         0x40000090
#define kHyperVMsrSIntVectorMask                0x00FFULL
#define kHyperVMsrSIntRsvd1Mask                 0xFF00ULL
#define kHyperVMsrSIntMasked                    0x00010000ULL
#define kHyperVMsrSIntAutoEoi                   0x00020000ULL
#define kHyperVMsrSIntRsvd2Mask                 0xFFFFFFFFFFFC0000ULL
#define kHyperVMsrSIntRsvdMask                  (kHyperVMsrSIntRsvd1Mask | kHyperVMsrSIntRsvd2Mask)

#define kHyperVMsrSTimer0Config                 0x400000B0
#define kHyperVMsrSTimerConfigEnable            0x0001ULL
#define kHyperVMsrSTimerConfigPeriodic          0x0002ULL
#define kHyperVMsrSTimerConfigLazy              0x0004ULL
#define kHyperVMsrSTimerConfigAutoEnable        0x0008ULL
#define kHyperVMsrSTimerConfigSIntMask          0x000F0000ULL
#define kHyperVMsrSTimerConfigSIntShift         16

#define kHyperVMsrSTimer0Count                  0x400000B1

//
// Message types
//
typedef enum : UInt32 {
  kHyperVMessageTypeNone          = 0x0,
  kHyperVMessageTypeChannel       = 0x1,
  kHyperVMessageTypeTimerExpired  = 0x80000010
} HyperVMessageType;

/*
 * Hypercall status codes
 */
#define HYPERCALL_STATUS_SUCCESS  0x0000

/*
 * Hypercall input values
 */
#define HYPERCALL_POST_MESSAGE    0x005c
#define HYPERCALL_SIGNAL_EVENT    0x005d

//
// Message posting
//
#define kHyperVMessageDataSizeMax     240
#define kHyperVMessageSize            256

typedef struct __attribute__((packed)) {
  UInt32              connectionId;
  UInt32              reserved;
  HyperVMessageType   messageType;
  UInt32              size;
  UInt8               data[kHyperVMessageSize];
} HyperVHypercallPostMessage;

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
  HyperVMessageType   type;
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
