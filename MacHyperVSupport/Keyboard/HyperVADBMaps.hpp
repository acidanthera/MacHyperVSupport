//
//  HyperVPS2ToADBMaps.hpp
//  Hyper-V keyboard driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperVADBMaps_hpp
#define HyperVADBMaps_hpp

#define kADBDeadKey               0x80

#define kADBConverterLength       256 * 2     // 0x00~0xff : normal key , 0x100~0x1ff : extended key
#define kADBConverterExStart      256

//
// PS/2 to ADB mappings. Used for standard keyboard input.
//
// PS/2 scancode reference : USB HID to PS/2 Scan Code Translation Table PS/2 Set 1 columns
// http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf
//
static const UInt8 PS2ToADBMapStock[kADBConverterLength] = {
/*  ADB        AT  ANSI Key-Legend
    ======================== */
    kADBDeadKey,// 00
    0x35,   // 01  Escape
    0x12,   // 02  1!
    0x13,   // 03  2@
    0x14,   // 04  3#
    0x15,   // 05  4$
    0x17,   // 06  5%
    0x16,   // 07  6^
    0x1a,   // 08  7&
    0x1c,   // 09  8*
    0x19,   // 0a  9(
    0x1d,   // 0b  0)
    0x1b,   // 0c  -_
    0x18,   // 0d  =+
    0x33,   // 0e  Backspace
    0x30,   // 0f  Tab
    0x0c,   // 10  qQ
    0x0d,   // 11  wW
    0x0e,   // 12  eE
    0x0f,   // 13  rR
    0x11,   // 14  tT
    0x10,   // 15  yY
    0x20,   // 16  uU
    0x22,   // 17  iI
    0x1f,   // 18  oO
    0x23,   // 19  pP
    0x21,   // 1a  [{
    0x1e,   // 1b  ]}
    0x24,   // 1c  Return
    0x3b,   // 1d  Left Control
    0x00,   // 1e  aA
    0x01,   // 1f  sS
    0x02,   // 20  dD
    0x03,   // 21  fF
    0x05,   // 22  gG
    0x04,   // 23  hH
    0x26,   // 24  jJ
    0x28,   // 25  kK
    0x25,   // 26  lL
    0x29,   // 27  ;:
    0x27,   // 28  '"
    0x32,   // 29  `~
    0x38,   // 2a  Left Shift
    0x2a,   // 2b  \| , Europe 1(ISO)
    0x06,   // 2c  zZ
    0x07,   // 2d  xX
    0x08,   // 2e  cC
    0x09,   // 2f  vV
    0x0b,   // 30  bB
    0x2d,   // 31  nN
    0x2e,   // 32  mM
    0x2b,   // 33  ,<
    0x2f,   // 34  .>
    0x2c,   // 35  /?
    0x3c,   // 36  Right Shift
    0x43,   // 37  Keypad *
    0x3a,   // 38  Left Alt
    0x31,   // 39  Space
    0x39,   // 3a  Caps Lock
    0x7a,   // 3b  F1
    0x78,   // 3c  F2
    0x63,   // 3d  F3
    0x76,   // 3e  F4
    0x60,   // 3f  F5
    0x61,   // 40  F6
    0x62,   // 41  F7
    0x64,   // 42  F8
    0x65,   // 43  F9
    0x6d,   // 44  F10
    0x47,   // 45  Num Lock
    0x6b,   // 46  Scroll Lock
    0x59,   // 47  Keypad 7 Home
    0x5b,   // 48  Keypad 8 Up
    0x5c,   // 49  Keypad 9 PageUp
    0x4e,   // 4a  Keypad -
    0x56,   // 4b  Keypad 4 Left
    0x57,   // 4c  Keypad 5
    0x58,   // 4d  Keypad 6 Right
    0x45,   // 4e  Keypad +
    0x53,   // 4f  Keypad 1 End
    0x54,   // 50  Keypad 2 Down
    0x55,   // 51  Keypad 3 PageDn
    0x52,   // 52  Keypad 0 Insert
    0x41,   // 53  Keypad . Delete
    0x44,   // 54  SysReq
    0x46,   // 55
    0x0a,   // 56  Europe 2(ISO)
    0x67,   // 57  F11
    0x6f,   // 58  F12
    0x51,   // 59  Keypad =
    kADBDeadKey,// 5a
    kADBDeadKey,// 5b
    0x5f,   // 5c  Keyboard Int'l 6 (PC9800 Keypad , )
    kADBDeadKey,// 5d
    kADBDeadKey,// 5e
    kADBDeadKey,// 5f
    kADBDeadKey,// 60
    kADBDeadKey,// 61
    kADBDeadKey,// 62
    kADBDeadKey,// 63
    0x69,   // 64  F13
    0x6b,   // 65  F14
    0x71,   // 66  F15
    0x6a,   // 67  F16
    0x40,   // 68  F17
    0x4f,   // 69  F18
    0x50,   // 6a  F19
    0x5a,   // 6b  F20
    kADBDeadKey,// 6c  F21
    kADBDeadKey,// 6d  F22
    kADBDeadKey,// 6e  F23
    kADBDeadKey,// 6f
    0x68,   // 70  Keyboard Intl'2 (Japanese Katakana/Hiragana)
    kADBDeadKey,// 71
    kADBDeadKey,// 72
    0x5e,   // 73  Keyboard Int'l 1 (Japanese Ro)
    kADBDeadKey,// 74
    kADBDeadKey,// 75
    kADBDeadKey,// 76  F24 , Keyboard Lang 5 (Japanese Zenkaku/Hankaku)
    0x68,   // 77  Keyboard Lang 4 (Japanese Hiragana)
    0x68,   // 78  Keyboard Lang 3 (Japanese Katakana)
    0x68,   // 79  Keyboard Int'l 4 (Japanese Henkan)
    kADBDeadKey,// 7a
    0x66,   // 7b  Keyboard Int'l 5 (Japanese Muhenkan)
    kADBDeadKey,// 7c
    0x5d,   // 7d  Keyboard Int'l 3 (Japanese Yen)
    0x5f,   // 7e  Keypad , (Brazilian Keypad .)
    kADBDeadKey,// 7f
    kADBDeadKey,// 80
    kADBDeadKey,// 81
    kADBDeadKey,// 82
    kADBDeadKey,// 83
    kADBDeadKey,// 84
    kADBDeadKey,// 85
    kADBDeadKey,// 86
    kADBDeadKey,// 87
    kADBDeadKey,// 88
    kADBDeadKey,// 89
    kADBDeadKey,// 8a
    kADBDeadKey,// 8b
    kADBDeadKey,// 8c
    kADBDeadKey,// 8d
    kADBDeadKey,// 8e
    kADBDeadKey,// 8f
    kADBDeadKey,// 90
    kADBDeadKey,// 91
    kADBDeadKey,// 92
    kADBDeadKey,// 93
    kADBDeadKey,// 94
    kADBDeadKey,// 95
    kADBDeadKey,// 96
    kADBDeadKey,// 97
    kADBDeadKey,// 98
    kADBDeadKey,// 99
    kADBDeadKey,// 9a
    kADBDeadKey,// 9b
    kADBDeadKey,// 9c
    kADBDeadKey,// 9d
    kADBDeadKey,// 9e
    kADBDeadKey,// 9f
    kADBDeadKey,// a0
    kADBDeadKey,// a1
    kADBDeadKey,// a2
    kADBDeadKey,// a3
    kADBDeadKey,// a4
    kADBDeadKey,// a5
    kADBDeadKey,// a6
    kADBDeadKey,// a7
    kADBDeadKey,// a8
    kADBDeadKey,// a9
    kADBDeadKey,// aa
    kADBDeadKey,// ab
    kADBDeadKey,// ac
    kADBDeadKey,// ad
    kADBDeadKey,// ae
    kADBDeadKey,// af
    kADBDeadKey,// b0
    kADBDeadKey,// b1
    kADBDeadKey,// b2
    kADBDeadKey,// b3
    kADBDeadKey,// b4
    kADBDeadKey,// b5
    kADBDeadKey,// b6
    kADBDeadKey,// b7
    kADBDeadKey,// b8
    kADBDeadKey,// b9
    kADBDeadKey,// ba
    kADBDeadKey,// bb
    kADBDeadKey,// bc
    kADBDeadKey,// bd
    kADBDeadKey,// be
    kADBDeadKey,// bf
    kADBDeadKey,// c0
    kADBDeadKey,// c1
    kADBDeadKey,// c2
    kADBDeadKey,// c3
    kADBDeadKey,// c4
    kADBDeadKey,// c5
    kADBDeadKey,// c6
    kADBDeadKey,// c7
    kADBDeadKey,// c8
    kADBDeadKey,// c9
    kADBDeadKey,// ca
    kADBDeadKey,// cb
    kADBDeadKey,// cc
    kADBDeadKey,// cd
    kADBDeadKey,// ce
    kADBDeadKey,// cf
    kADBDeadKey,// d0
    kADBDeadKey,// d1
    kADBDeadKey,// d2
    kADBDeadKey,// d3
    kADBDeadKey,// d4
    kADBDeadKey,// d5
    kADBDeadKey,// d6
    kADBDeadKey,// d7
    kADBDeadKey,// d8
    kADBDeadKey,// d9
    kADBDeadKey,// da
    kADBDeadKey,// db
    kADBDeadKey,// dc
    kADBDeadKey,// dd
    kADBDeadKey,// de
    kADBDeadKey,// df
    kADBDeadKey,// e0
    kADBDeadKey,// e1
    kADBDeadKey,// e2
    kADBDeadKey,// e3
    kADBDeadKey,// e4
    kADBDeadKey,// e5
    kADBDeadKey,// e6
    kADBDeadKey,// e7
    kADBDeadKey,// e8
    kADBDeadKey,// e9
    kADBDeadKey,// ea
    kADBDeadKey,// eb
    kADBDeadKey,// ec
    kADBDeadKey,// ed
    kADBDeadKey,// ee
    kADBDeadKey,// ef
    kADBDeadKey,// f0
    0x66,   // f1*  Keyboard Lang 2 (Korean Hanja)
    0x68,   // f2*  Keyboard Lang 1 (Korean Hangul)
    kADBDeadKey,// f3
    kADBDeadKey,// f4
    kADBDeadKey,// f5
    kADBDeadKey,// f6
    kADBDeadKey,// f7
    kADBDeadKey,// f8
    kADBDeadKey,// f9
    kADBDeadKey,// fa
    kADBDeadKey,// fb
    kADBDeadKey,// fc
    kADBDeadKey,// fd
    kADBDeadKey,// fe
    kADBDeadKey,// ff
    kADBDeadKey,// e0 00
    kADBDeadKey,// e0 01
    kADBDeadKey,// e0 02
    kADBDeadKey,// e0 03
    kADBDeadKey,// e0 04
    kADBDeadKey,// e0 05
    kADBDeadKey,// e0 06
    kADBDeadKey,// e0 07
    kADBDeadKey,// e0 08
    kADBDeadKey,// e0 09
    kADBDeadKey,// e0 0a
    kADBDeadKey,// e0 0b
    kADBDeadKey,// e0 0c
    kADBDeadKey,// e0 0d
    kADBDeadKey,// e0 0e
    kADBDeadKey,// e0 0f
    kADBDeadKey,// e0 10
    kADBDeadKey,// e0 11
    kADBDeadKey,// e0 12
    kADBDeadKey,// e0 13
    kADBDeadKey,// e0 14
    kADBDeadKey,// e0 15
    kADBDeadKey,// e0 16
    kADBDeadKey,// e0 17
    kADBDeadKey,// e0 18
    kADBDeadKey,// e0 19
    kADBDeadKey,// e0 1a
    kADBDeadKey,// e0 1b
    0x4c,   // e0 1c  Keypad Enter
    0x3e,   // e0 1d  Right Control
    kADBDeadKey,// e0 1e
    kADBDeadKey,// e0 1f
    kADBDeadKey,// e0 20  Mute
    kADBDeadKey,// e0 21  Calculator
    kADBDeadKey,// e0 22  Play/Pause
    kADBDeadKey,// e0 23
    kADBDeadKey,// e0 24  Stop
    kADBDeadKey,// e0 25
    kADBDeadKey,// e0 26
    kADBDeadKey,// e0 27
    kADBDeadKey,// e0 28
    kADBDeadKey,// e0 29
    kADBDeadKey,// e0 2a
    kADBDeadKey,// e0 2b
    kADBDeadKey,// e0 2c
    kADBDeadKey,// e0 2d
    kADBDeadKey,// e0 2e
    kADBDeadKey,// e0 2f
    kADBDeadKey,// e0 30
    kADBDeadKey,// e0 31
    kADBDeadKey,// e0 32  WWW Home
    kADBDeadKey,// e0 33
    kADBDeadKey,// e0 34
    0x4b,   // e0 35  Keypad /
    kADBDeadKey,// e0 36
    0x69,   // e0 37  Print Screen
    0x3d,   // e0 38  Right Alt
    kADBDeadKey,// e0 39
    kADBDeadKey,// e0 3a
    kADBDeadKey,// e0 3b
    kADBDeadKey,// e0 3c
    kADBDeadKey,// e0 3d
    kADBDeadKey,// e0 3e
    kADBDeadKey,// e0 3f
    kADBDeadKey,// e0 40
    kADBDeadKey,// e0 41
    kADBDeadKey,// e0 42
    kADBDeadKey,// e0 43
    kADBDeadKey,// e0 44
    0x71,   // e0 45* Pause
    kADBDeadKey,// e0 46* Break(Ctrl-Pause)
    0x73,   // e0 47  Home
    0x7e,   // e0 48  Up Arrow
    0x74,   // e0 49  Page Up
    kADBDeadKey,// e0 4a
    0x7b,   // e0 4b  Left Arrow
    kADBDeadKey,// e0 4c
    0x7c,   // e0 4d  Right Arrow
    kADBDeadKey,// e0 4e
    0x77,   // e0 4f  End
    0x7d,   // e0 50  Down Arrow
    0x79,   // e0 51  Page Down
    kADBDeadKey,// e0 52  Insert = Eject
    0x75,   // e0 53  Delete
    kADBDeadKey,// e0 54
    kADBDeadKey,// e0 55
    kADBDeadKey,// e0 56
    kADBDeadKey,// e0 57
    kADBDeadKey,// e0 58
    kADBDeadKey,// e0 59
    kADBDeadKey,// e0 5a
    0x37,   // e0 5b  Left GUI(Windows)
    0x36,   // e0 5c  Right GUI(Windows)
    0x6e,   // e0 5d  App( Windows context menu key )
    kADBDeadKey,// e0 5e  System Power / Keyboard Power
    kADBDeadKey,// e0 5f  System Sleep (hp Fn+F1)
    kADBDeadKey,// e0 60
    kADBDeadKey,// e0 61
    kADBDeadKey,// e0 62
    kADBDeadKey,// e0 63  System Wake
    kADBDeadKey,// e0 64
    kADBDeadKey,// e0 65  WWW Search
    kADBDeadKey,// e0 66  WWW Favorites
    kADBDeadKey,// e0 67  WWW Refresh
    kADBDeadKey,// e0 68  WWW Stop
    kADBDeadKey,// e0 69  WWW Forward
    kADBDeadKey,// e0 6a  WWW Back
    kADBDeadKey,// e0 6b  My Computer
    kADBDeadKey,// e0 6c  Mail
    kADBDeadKey,// e0 6d  Media Select
    kADBDeadKey,// e0 6e
    kADBDeadKey,// e0 6f
    kADBDeadKey,// e0 70
    kADBDeadKey,// e0 71
    kADBDeadKey,// e0 72
    kADBDeadKey,// e0 73
    kADBDeadKey,// e0 74
    kADBDeadKey,// e0 75
    kADBDeadKey,// e0 76
    kADBDeadKey,// e0 77
    kADBDeadKey,// e0 78
    kADBDeadKey,// e0 79
    kADBDeadKey,// e0 7a
    kADBDeadKey,// e0 7b
    kADBDeadKey,// e0 7c
    kADBDeadKey,// e0 7d
    kADBDeadKey,// e0 7e
    kADBDeadKey,// e0 7f
    kADBDeadKey,// e0 80
    kADBDeadKey,// e0 81
    kADBDeadKey,// e0 82
    kADBDeadKey,// e0 83
    kADBDeadKey,// e0 84
    kADBDeadKey,// e0 85
    kADBDeadKey,// e0 86
    kADBDeadKey,// e0 87
    kADBDeadKey,// e0 88
    kADBDeadKey,// e0 89
    kADBDeadKey,// e0 8a
    kADBDeadKey,// e0 8b
    kADBDeadKey,// e0 8c
    kADBDeadKey,// e0 8d
    kADBDeadKey,// e0 8e
    kADBDeadKey,// e0 8f
    kADBDeadKey,// e0 90
    kADBDeadKey,// e0 91
    kADBDeadKey,// e0 92
    kADBDeadKey,// e0 93
    kADBDeadKey,// e0 94
    kADBDeadKey,// e0 95
    kADBDeadKey,// e0 96
    kADBDeadKey,// e0 97
    kADBDeadKey,// e0 98
    kADBDeadKey,// e0 99
    kADBDeadKey,// e0 9a
    kADBDeadKey,// e0 9b
    kADBDeadKey,// e0 9c
    kADBDeadKey,// e0 9d
    kADBDeadKey,// e0 9e
    kADBDeadKey,// e0 9f
    kADBDeadKey,// e0 a0
    kADBDeadKey,// e0 a1
    kADBDeadKey,// e0 a2
    kADBDeadKey,// e0 a3
    kADBDeadKey,// e0 a4
    kADBDeadKey,// e0 a5
    kADBDeadKey,// e0 a6
    kADBDeadKey,// e0 a7
    kADBDeadKey,// e0 a8
    kADBDeadKey,// e0 a9
    kADBDeadKey,// e0 aa
    kADBDeadKey,// e0 ab
    kADBDeadKey,// e0 ac
    kADBDeadKey,// e0 ad
    kADBDeadKey,// e0 ae
    kADBDeadKey,// e0 af
    kADBDeadKey,// e0 b0
    kADBDeadKey,// e0 b1
    kADBDeadKey,// e0 b2
    kADBDeadKey,// e0 b3
    kADBDeadKey,// e0 b4
    kADBDeadKey,// e0 b5
    kADBDeadKey,// e0 b6
    kADBDeadKey,// e0 b7
    kADBDeadKey,// e0 b8
    kADBDeadKey,// e0 b9
    kADBDeadKey,// e0 ba
    kADBDeadKey,// e0 bb
    kADBDeadKey,// e0 bc
    kADBDeadKey,// e0 bd
    kADBDeadKey,// e0 be
    kADBDeadKey,// e0 bf
    kADBDeadKey,// e0 c0
    kADBDeadKey,// e0 c1
    kADBDeadKey,// e0 c2
    kADBDeadKey,// e0 c3
    kADBDeadKey,// e0 c4
    kADBDeadKey,// e0 c5
    kADBDeadKey,// e0 c6
    kADBDeadKey,// e0 c7
    kADBDeadKey,// e0 c8
    kADBDeadKey,// e0 c9
    kADBDeadKey,// e0 ca
    kADBDeadKey,// e0 cb
    kADBDeadKey,// e0 cc
    kADBDeadKey,// e0 cd
    kADBDeadKey,// e0 ce
    kADBDeadKey,// e0 cf
    kADBDeadKey,// e0 d0
    kADBDeadKey,// e0 d1
    kADBDeadKey,// e0 d2
    kADBDeadKey,// e0 d3
    kADBDeadKey,// e0 d4
    kADBDeadKey,// e0 d5
    kADBDeadKey,// e0 d6
    kADBDeadKey,// e0 d7
    kADBDeadKey,// e0 d8
    kADBDeadKey,// e0 d9
    kADBDeadKey,// e0 da
    kADBDeadKey,// e0 db
    kADBDeadKey,// e0 dc
    kADBDeadKey,// e0 dd
    kADBDeadKey,// e0 de
    kADBDeadKey,// e0 df
    kADBDeadKey,// e0 e0
    kADBDeadKey,// e0 e1
    kADBDeadKey,// e0 e2
    kADBDeadKey,// e0 e3
    kADBDeadKey,// e0 e4
    kADBDeadKey,// e0 e5
    kADBDeadKey,// e0 e6
    kADBDeadKey,// e0 e7
    kADBDeadKey,// e0 e8
    kADBDeadKey,// e0 e9
    kADBDeadKey,// e0 ea
    kADBDeadKey,// e0 eb
    kADBDeadKey,// e0 ec
    kADBDeadKey,// e0 ed
    kADBDeadKey,// e0 ee
    kADBDeadKey,// e0 ef
    kADBDeadKey,// e0 f0
    kADBDeadKey,// e0 f1
    kADBDeadKey,// e0 f2
    kADBDeadKey,// e0 f3
    kADBDeadKey,// e0 f4
    kADBDeadKey,// e0 f5
    kADBDeadKey,// e0 f6
    kADBDeadKey,// e0 f7
    kADBDeadKey,// e0 f8
    kADBDeadKey,// e0 f9
    kADBDeadKey,// e0 fa
    kADBDeadKey,// e0 fb
    kADBDeadKey,// e0 fc
    kADBDeadKey,// e0 fd
    kADBDeadKey,// e0 fe
    kADBDeadKey // e0 ff
};

//
// Unicode to ADB mappings. Used for Hyper-V's "type clipboard text" "feature".
//
#define kADBUnicodeShift            0xF000
#define kADBKeyCodeShift            0x38

static const UInt16 UnicodeToADBMap[0x80] = {
  //
  // Control characters.
  //
  kADBDeadKey,  // 00 NUL
  kADBDeadKey,  // 01 SOH
  kADBDeadKey,  // 02 STX
  kADBDeadKey,  // 03 ETX
  kADBDeadKey,  // 04 EOT
  kADBDeadKey,  // 05 ENQ
  kADBDeadKey,  // 06 ACK
  kADBDeadKey,  // 07 BEL
  0x33,         // 08 Backspace
  0x30,         // 09 Tab
  kADBDeadKey,  // 0A Line Feed
  kADBDeadKey,  // 0B VT
  kADBDeadKey,  // 0C Form Feed
  0x24,         // 0D Carriage Return
  kADBDeadKey,  // 0E SO
  kADBDeadKey,  // 0F SI

  kADBDeadKey,  // 10 DLE
  kADBDeadKey,  // 11 DC1
  kADBDeadKey,  // 12 DC2
  kADBDeadKey,  // 13 DC3
  kADBDeadKey,  // 14 DC4
  kADBDeadKey,  // 15 NAK
  kADBDeadKey,  // 16 SYN
  kADBDeadKey,  // 17 ETB
  kADBDeadKey,  // 18 CAN
  kADBDeadKey,  // 19 EM
  kADBDeadKey,  // 1A SUB
  0x35,         // 1B Escape
  kADBDeadKey,  // 1C FS
  kADBDeadKey,  // 1D GS
  kADBDeadKey,  // 1E GS
  kADBDeadKey,  // 1F US

  //
  // ASCII characters.
  //
  0x31,                     // 20 Space
  0x12 | kADBUnicodeShift,  // 21 !
  0x27 | kADBUnicodeShift,  // 22 "
  0x14 | kADBUnicodeShift,  // 23 #
  0x15 | kADBUnicodeShift,  // 24 $
  0x17 | kADBUnicodeShift,  // 25 %
  0x1A | kADBUnicodeShift,  // 26 &
  0x27,                     // 27 '
  0x19 | kADBUnicodeShift,  // 28 (
  0x1D | kADBUnicodeShift,  // 29 )
  0x1C | kADBUnicodeShift,  // 2A *
  0x18 | kADBUnicodeShift,  // 2B +
  0x2B,                     // 2C ,
  0x1B,                     // 2D -
  0x2F,                     // 2E .
  0x2C,                     // 2F /

  0x1D,                     // 30 0
  0x12,                     // 31 1
  0x13,                     // 32 2
  0x14,                     // 33 3
  0x15,                     // 34 4
  0x17,                     // 35 5
  0x16,                     // 36 6
  0x1A,                     // 37 7
  0x1C,                     // 38 8
  0x19,                     // 39 9
  0x29 | kADBUnicodeShift,  // 3A :
  0x29,                     // 3B ;
  0x2B | kADBUnicodeShift,  // 3C <
  0x18,                     // 3D =
  0x2F | kADBUnicodeShift,  // 3E >
  0x2C | kADBUnicodeShift,  // 3F ?

  0x13 | kADBUnicodeShift,  // 40 @
  0x00 | kADBUnicodeShift,  // 41 A
  0x0B | kADBUnicodeShift,  // 42 B
  0x08 | kADBUnicodeShift,  // 43 C
  0x02 | kADBUnicodeShift,  // 44 D
  0x0E | kADBUnicodeShift,  // 45 E
  0x03 | kADBUnicodeShift,  // 46 F
  0x05 | kADBUnicodeShift,  // 47 G
  0x04 | kADBUnicodeShift,  // 48 H
  0x22 | kADBUnicodeShift,  // 49 I
  0x26 | kADBUnicodeShift,  // 4A J
  0x28 | kADBUnicodeShift,  // 4B K
  0x25 | kADBUnicodeShift,  // 4C L
  0x2E | kADBUnicodeShift,  // 4D M
  0x2D | kADBUnicodeShift,  // 4E N
  0x1F | kADBUnicodeShift,  // 4F O

  0x23 | kADBUnicodeShift,  // 50 P
  0x0C | kADBUnicodeShift,  // 51 Q
  0x0F | kADBUnicodeShift,  // 52 R
  0x01 | kADBUnicodeShift,  // 53 S
  0x11 | kADBUnicodeShift,  // 54 T
  0x20 | kADBUnicodeShift,  // 55 U
  0x09 | kADBUnicodeShift,  // 56 V
  0x0D | kADBUnicodeShift,  // 57 W
  0x07 | kADBUnicodeShift,  // 58 X
  0x10 | kADBUnicodeShift,  // 59 Y
  0x06 | kADBUnicodeShift,  // 5A Z
  0x21,                     // 5B [
  0x2A,                     // 5C \ Backslash
  0x1E,                     // 5D ]
  0x16 | kADBUnicodeShift,  // 5E ^
  0x1B | kADBUnicodeShift,  // 5F _

  0x32,                     // 60 `
  0x00,                     // 61 a
  0x0B,                     // 62 b
  0x08,                     // 63 c
  0x02,                     // 64 d
  0x0E,                     // 65 e
  0x03,                     // 66 f
  0x05,                     // 67 g
  0x04,                     // 68 h
  0x22,                     // 69 i
  0x26,                     // 6A j
  0x28,                     // 6B k
  0x25,                     // 6C l
  0x2E,                     // 6D m
  0x2D,                     // 6E n
  0x1F,                     // 6F o

  0x23,                     // 70 p
  0x0C,                     // 71 q
  0x0F,                     // 72 r
  0x01,                     // 73 s
  0x11,                     // 74 t
  0x20,                     // 75 u
  0x09,                     // 76 v
  0x0D,                     // 77 w
  0x07,                     // 78 x
  0x10,                     // 79 y
  0x06,                     // 7A z
  0x21 | kADBUnicodeShift,  // 7B {
  0x2A | kADBUnicodeShift,  // 7C |
  0x1E | kADBUnicodeShift,  // 7D }
  0x32 | kADBUnicodeShift,  // 7E ~
  0x75,                     // 7F DEL
};

#endif
