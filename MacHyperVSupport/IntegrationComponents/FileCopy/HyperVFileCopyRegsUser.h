//
//  HyperVFileCopyRegsUser.h
//  Hyper-V file copy driver
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVFileCopyRegsUser_h
#define HyperVFileCopyRegsUser_h

#define kHyperVFileCopyMaxPath      260
#define kHyperVFileCopyFragmentSize (6 * 1024)

//
// File copy message types.
//
typedef enum : UInt32 {
  kHyperVFileCopyMessageTypeStartCopy    = 0,
  kHyperVFileCopyMessageTypeWriteToFile  = 1,
  kHyperVFileCopyMessageTypeCompleteCopy = 2,
  kHyperVFileCopyMessageTypeCancelCopy   = 3
} HyperVFileCopyMessageType;

//
// File copy flags.
//
typedef enum : UInt32 {
  kHyperVFileCopyMessageFlagsOverwrite  = 1,
  kHyperVFileCopyMessageFlagsCreatePath = 2
} HyperVFileCopyMessageFlags;

#endif
