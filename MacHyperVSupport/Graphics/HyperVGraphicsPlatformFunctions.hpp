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
#define kHyperVGraphicsPlatformFunctionSetCursorShape     "HyperVGraphicsPlatformFunctionSetCursorShape"
#define kHyperVGraphicsPlatformFunctionSetCursorPosition  "HyperVGraphicsPlatformFunctionSetCursorPosition"

//
// HyperVGraphicsPlatformFunctionSetCursorShape parameters
//
typedef struct {
  UInt8   *cursorData;
  UInt32  width;
  UInt32  height;
  UInt32  hotX;
  UInt32  hotY;
} HyperVGraphicsPlatformFunctionSetCursorShapeParams;

#endif
