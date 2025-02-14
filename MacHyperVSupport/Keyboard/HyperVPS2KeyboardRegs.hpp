//
//  HyperVPS2KeyboardRegs.hpp
//  Hyper-V PS/2 keyboard driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVPS2KeyboardRegs_hpp
#define HyperVPS2KeyboardRegs_hpp

#define kHyperVPS2DataPort      0x60
#define kHyperVPS2CommandPort   0x64

#define kHyperVPS2Delay     10

#define kHyperVPS2SelfTestPassResult      0x55
#define kHyperVPS2KeyboardPortPassResult  0x00

//
// PS/2 status bits.
//
#define kHyperVPS2StatusOutputBusy      BIT(0)
#define kHyperVPS2StatusInputBusy       BIT(1)
#define kHyperVPS2StatusSystem          BIT(2)
#define kHyperVPS2StatusControllerData  BIT(3)
#define kHyperVPS2StatusTimeout         BIT(6)
#define kHyperVPS2StatusParity          BIT(7)

//
// PS/2 controller configuration register bits.
//
#define kHyperVPS2ConfigEnableKeyboardIRQ     BIT(0)
#define kHyperVPS2ConfigEnableMouseIRQ        BIT(1)
#define kHyperVPS2ConfigSystem                BIT(2)
#define kHyperVPS2ConfigKeyboardDisableClock  BIT(4)
#define kHyperVPS2ConfigMouseDisableClock     BIT(5)
#define kHyperVPS2ConfigKeyboardTranslation   BIT(6)

//
// PS/2 controller commands.
//
#define kHyperVPS2CommandReadByte             0x20
#define kHyperVPS2CommandReadByteBase         0x21
#define kHyperVPS2CommandWriteByte            0x60
#define kHyperVPS2CommandWriteByteBase        0x61
#define kHyperVPS2CommandDisableMousePort     0xA7
#define kHyperVPS2CommandEnableMousePort      0xA8
#define kHyperVPS2CommandTestMousePort        0xA9
#define kHyperVPS2CommandTestController       0xAA
#define kHyperVPS2CommandTestKeyboardPort     0xAB
#define kHyperVPS2CommandDiagnosticDump       0xAC
#define kHyperVPS2CommandDisableKeyboardPort  0xAD
#define kHyperVPS2CommandEnableKeyboardPort   0xAE
#define kHyperVPS2CommandReadInput            0xC0
#define kHyperVPS2CommandPollInputLow         0xC1
#define kHyperVPS2CommandPollInputHigh        0xC2
#define kHyperVPS2CommandReadOutput           0xD0
#define kHyperVPS2CommandWriteOutput          0xD1
#define kHyperVPS2CommandWriteOutputKeyboard  0xD2
#define kHyperVPS2CommandWriteOutputMouse     0xD3
#define kHyperVPS2CommandWriteInputMouse      0xD4
#define kHyperVPS2CommandPulseOutputBase      0xF0

//
// PS/2 keyboard commands.
//
#define kHyperVPS2KeyboardAck                 0xFA

#define kHyperVPS2KeyboardCommandEnable       0xF4
#define kHyperVPS2KeyboardCommandSetDefaults  0xF6

//
// PS/2 scancode modifiers.
//
#define kHyperVPS2KeyboardScancodeBreak       0x80
#define kHyperVPS2KeyboardE0                  0xE0
#define kHyperVPS2KeyboardE1                  0xE1

#endif
