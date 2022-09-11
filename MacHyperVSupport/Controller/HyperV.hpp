//
//  HyperV.hpp
//  Hyper-V register and structures header
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperV_hpp
#define HyperV_hpp

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/IOLib.h>

#include <Headers/kern_api.hpp>

#define kHyperVStatusSuccess    0
#define kHyperVStatusFail       0x80004005

#define BIT(a)                  (1 << (a))

#define HV_PAGEALIGN(a)         (((a) + (PAGE_SIZE - 1)) &~ (PAGE_SIZE - 1))

#define kHyperVHypercallRetryCount  100


#define kHyperVPCIBusSyntheticGraphics    0xFB
#define kHyperVPCIBusDummy                0xFE

#if DEBUG
//
// Debug logging function.
//
inline void logPrint(const char *className, const char *funcName, bool hasChannelId, UInt32 channelId, const char *format, va_list va) {
  char tmp[256];
  tmp[0] = '\0';
  vsnprintf(tmp, sizeof (tmp), format, va);
  
  if (hasChannelId) {
    IOLog("%s(%u)::%s(): %s\n", className, (unsigned int) channelId, funcName, tmp);
  } else {
    IOLog("%s::%s(): %s\n", className, funcName, tmp);
  }
}

//
// Log functions for normal modules.
//
#define HVDeclareLogFunctions(a) \
  private: \
  bool debugEnabled = false; \
  inline void HVCheckDebugArgs() { \
    debugEnabled = checkKernelArgument("-hv" a "dbg"); \
  } \
  inline void HVDBGLOG_PRINT(const char *func, const char *str, ...) const { \
    if (this->debugEnabled) { \
      va_list args; \
      va_start(args, str); \
      logPrint(this->getMetaClass()->getClassName(), func, false, 0, str, args); \
      va_end(args); \
    } \
  } \
    \
  inline void HVSYSLOG_PRINT(const char *func, const char *str, ...) const { \
    va_list args; \
    va_start(args, str); \
    logPrint(this->getMetaClass()->getClassName(), func, false, 0, str, args); \
    va_end(args); \
  } \
  protected:

//
// Log functions for VMBus child modules.
//
#define HVDeclareLogFunctionsVMBusChild(a) \
  private: \
  bool debugEnabled = false; \
  inline bool HVCheckOffArg() { \
    return checkKernelArgument("-hv" a "off");; \
  } \
  inline void HVCheckDebugArgs() { \
    debugEnabled = checkKernelArgument("-hv" a "dbg"); \
    hvDevice->setDebugMessagePrinting(checkKernelArgument("-hv" a "msgdbg")); \
    if (checkKernelArgument("-hv" a "statsdbg")) { hvDevice->enableTimerDebugPrints(); } \
  } \
  inline void HVDBGLOG_PRINT(const char *func, const char *str, ...) const { \
    if (debugEnabled) { \
      va_list args; \
      va_start(args, str); \
      logPrint(getMetaClass()->getClassName(), func, true, hvDevice != nullptr ? hvDevice->getChannelId() : -1, str, args); \
      va_end(args); \
    } \
  } \
    \
  inline void HVSYSLOG_PRINT(const char *func, const char *str, ...) const { \
    va_list args; \
    va_start(args, str); \
    logPrint(getMetaClass()->getClassName(), func, true, hvDevice != nullptr ? hvDevice->getChannelId() : -1, str, args); \
    va_end(args); \
  } \
  protected:

//
// Log functions for the VMBus device nub module.
//
#define HVDeclareLogFunctionsVMBusDeviceNub(a) \
  private: \
  bool debugEnabled = false; \
  bool debugPackets = false; \
  inline void HVCheckDebugArgs() { \
    debugEnabled = checkKernelArgument("-hv" a "dbg"); \
  } \
  inline void HVDBGLOG_PRINT(const char *func, const char *str, ...) const { \
    if (this->debugEnabled) { \
      va_list args; \
      va_start(args, str); \
      logPrint(this->getMetaClass()->getClassName(), func, true, this->_channelId, str, args); \
      va_end(args); \
    } \
  } \
    \
  inline void HVSYSLOG_PRINT(const char *func, const char *str, ...) const { \
    va_list args; \
    va_start(args, str); \
    logPrint(this->getMetaClass()->getClassName(), func, true, this->_channelId, str, args); \
    va_end(args); \
  } \
    \
  inline void HVMSGLOG_PRINT(const char *func, const char *str, ...) const { \
    if (this->debugPackets) { \
      va_list args; \
      va_start(args, str); \
      logPrint(this->getMetaClass()->getClassName(), func, true, this->_channelId, str, args); \
      va_end(args); \
    } \
  } \
  protected:

//
// Common logging macros to inject function name.
//
#define HVDBGLOG(str, ...) HVDBGLOG_PRINT(__FUNCTION__, str, ## __VA_ARGS__)
#define HVSYSLOG(str, ...) HVSYSLOG_PRINT(__FUNCTION__, str, ## __VA_ARGS__)
#define HVMSGLOG(str, ...) HVMSGLOG_PRINT(__FUNCTION__, str, ## __VA_ARGS__)

#else

//
// Release print function.
//
inline void logPrint(const char *className, bool hasChannelId, UInt32 channelId, const char *format, va_list va) {
  char tmp[256];
  tmp[0] = '\0';
  vsnprintf(tmp, sizeof (tmp), format, va);
  
  if (hasChannelId) {
    IOLog("%s(%u): %s\n", className, (unsigned int) channelId, tmp);
  } else {
    IOLog("%s: %s\n", className, tmp);
  }
}

//
// Log functions for normal modules.
//
#define HVDeclareLogFunctions(a) \
  private: \
  bool debugEnabled = false; \
  inline void HVCheckDebugArgs() { } \
  inline void HVDBGLOG(const char *str, ...) const { } \
    \
  inline void HVSYSLOG(const char *str, ...) const { \
    va_list args; \
    va_start(args, str); \
    logPrint(this->getMetaClass()->getClassName(), false, 0, str, args); \
    va_end(args); \
  } \
  protected:

//
// Log functions for VMBus child modules.
//
#define HVDeclareLogFunctionsVMBusChild(a) \
  private: \
  bool debugEnabled = false; \
  inline bool HVCheckOffArg() { \
    return checkKernelArgument("-hv" a "off");; \
  } \
  inline void HVCheckDebugArgs() { } \
  inline void HVDBGLOG(const char *str, ...) const { } \
    \
  inline void HVSYSLOG(const char *str, ...) const { \
    va_list args; \
    va_start(args, str); \
    logPrint(this->getMetaClass()->getClassName(), true, hvDevice != nullptr ? hvDevice->getChannelId() : -1, str, args); \
    va_end(args); \
  } \
  protected:

//
// Log functions for the VMBus device nub module.
//
#define HVDeclareLogFunctionsVMBusDeviceNub(a) \
  private: \
  bool debugEnabled = false; \
  bool debugPackets = false; \
  inline void HVCheckDebugArgs() { } \
  inline void HVDBGLOG(const char *str, ...) const { } \
    \
  inline void HVSYSLOG(const char *str, ...) const { \
    va_list args; \
    va_start(args, str); \
    logPrint(this->getMetaClass()->getClassName(), true, this->_channelId, str, args); \
    va_end(args); \
  } \
    \
  inline void HVMSGLOG(const char *str, ...) const { } \
  protected:
#endif

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_12_0
#define HV_PCIBRIDGE_CLASS IOPCIHostBridge
#else
#define HV_PCIBRIDGE_CLASS IOPCIBridge
#endif

//
// Bit functions.
//
#define ADDR (*(volatile long *)addr)

static inline void sync_set_bit(long nr, volatile void *addr) {
  asm volatile("lock; bts %1,%0"
         : "+m" (ADDR)
         : "Ir" (nr)
         : "memory");
}

/**
 * sync_clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * sync_clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_atomic() and/or smp_mb__after_atomic()
 * in order to ensure changes are visible on other processors.
 */
static inline void sync_clear_bit(long nr, volatile void *addr)
{
  asm volatile("lock; btr %1,%0"
         : "+m" (ADDR)
         : "Ir" (nr)
         : "memory");
}

/**
 * sync_change_bit - Toggle a bit in memory
 * @nr: Bit to change
 * @addr: Address to start counting from
 *
 * sync_change_bit() is atomic and may not be reordered.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static inline void sync_change_bit(long nr, volatile void *addr)
{
  asm volatile("lock; btc %1,%0"
         : "+m" (ADDR)
         : "Ir" (nr)
         : "memory");
}

/**
 * sync_test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static inline int sync_test_and_set_bit(long nr, volatile void *addr)
{
  int oldbit;

  asm volatile("lock; bts %2,%1\n\tsbbl %0,%0"
         : "=r" (oldbit), "+m" (ADDR)
         : "Ir" (nr) : "memory");
  return oldbit;
}

/**
 * sync_test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static inline int sync_test_and_clear_bit(long nr, volatile void *addr)
{
  int oldbit;

  asm volatile("lock; btr %2,%1\n\tsbbl %0,%0"
         : "=r" (oldbit), "+m" (ADDR)
         : "Ir" (nr) : "memory");
  return oldbit;
}

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

//
// Hypercall status codes and input values
//
// This driver only uses HvPostMessage and HvSignalEvent.
//
#define kHypercallStatusSuccess               0x0000
#define kHypercallStatusInvalidParameter      0x0005
#define kHypercallStatusInsufficientMemory    0x000B
#define kHypercallStatusInvalidConnectionId   0x0012
#define kHypercallStatusInsufficientBuffers   0x0013 // TLFS has this incorrectly as 0x33

#define kHypercallTypePostMessage   0x0005C // Slow hypercall, memory-based
#define kHypercallTypeSignalEvent   0x1005D // Fast hypercall, register-based

#define kHypercallStatusMask        0xFFFF

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
  UInt8               data[kHyperVMessageDataSizeMax];
} HypercallPostMessage;

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
  volatile HyperVMessageType    type;
  UInt8                         size;
  volatile HyperVMessageFlags   flags;
  UInt16                        reserved;
  union {
    UInt64                      sender;
    HyperVPortId                portId;
  };
  UInt8                         data[kHyperVMessageDataSizeMax];
} HyperVMessage;

//
// GPADL range
//
typedef struct {
  UInt32  byteCount;
  UInt32  byteOffset;
  UInt64  pfnArray[];
} HyperVGPARange;

#define kHyperVEventFlagsByteCount  256
#define kHyperVEventFlagsDwordCount (kHyperVEventFlagsByteCount / sizeof (UInt32))

//
// Event flags.
//
typedef struct __attribute__((packed)) {
  union {
    UInt8   flags8[kHyperVEventFlagsByteCount];
    UInt32  flags32[kHyperVEventFlagsDwordCount];
  };
} HyperVEventFlags;

typedef struct __attribute__((packed)) {
  UInt32 connectionId;
  UInt16 eventFlagsOffset;
  UInt16 reserved;
} HyperVMonitorNotificationParameter;

//
// DMA buffer structure.
//
typedef struct {
  IOBufferMemoryDescriptor  *bufDesc;
  IODMACommand              *dmaCmd;
  mach_vm_address_t         physAddr;
  void                      *buffer;
  size_t                    size;
} HyperVDMABuffer;

//
// XNU CPU external functions/variables from mp.c.
//
extern unsigned int real_ncpus;    /* real number of cpus */
extern "C" {
  int cpu_number(void);
}

#endif
