//
//  HyperVGraphicsPlatformFunctions.hpp
//  Hyper-V synthetic graphics driver platform function definitions
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVGraphicsPlatformFunctions_hpp
#define HyperVGraphicsPlatformFunctions_hpp

//
// Platform function names.
//
#define kHyperVGraphicsPlatformFunctionInit               "HyperVGraphicsPlatformFunctionInit"
#define kHyperVGraphicsPlatformFunctionSetResolution      "HyperVGraphicsPlatformFunctionSetResolution"

#define kHyperVGraphicsFunctionSetCursor        "HyperVGraphicsFunctionSetCursor"
#define kHyperVGraphicsFunctionSetCusorPosition "HyperVGraphicsFunctionSetCusorPosition"

//
// HyperVGraphicsFunctionSetCursor parameters
//
typedef struct {
  UInt8   *cursorData;
  UInt32  width;
  UInt32  height;
  UInt32  hotX;
  UInt32  hotY;
} HyperVGraphicsFunctionSetCursorParams;

#endif
