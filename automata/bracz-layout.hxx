// Declarations of structures for the hardware devices used on the layout of
// bracz.

#ifndef _AUTOMATA_BRACZ_LAYOUT_HXX_
#define _AUTOMATA_BRACZ_LAYOUT_HXX_

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"
#include "../cs/src/base.h"

using namespace automata;

/*
  Bracz-layout assignment:

  LAYOUT | 0x2[1-7]zz = i2c extender board 1-7.

  zz = 00-0F: 8 bits for IOA[0].
  00, 01: LED1 (red) (C0)
  02, 03: LED2 (gre) (C1)
  the rest are unused

  zz = 10-1F: 8 bits for IOA[1]
  10, 11: A0 ACT_ORA_RED
  12, 13: A2 ACT_ORA_GREEN
  14, 15: C4 ACT_GREEN_GREEN
  16, 17: C6 ACT_GREEN_RED
  18, 19: C2 ACT_BLUE_BROWN
  1a, 1b: A1 ACT_BLUE_GREY  -- check if works.
  1c, 1d: C3 REL_GREEN [right]
  1e, 1f: C7 REL_BLUE [left]

  zz = 20-2F: 8 bits for input IOB
  20, 21: B5 IN_ORA_GREEN
  22, 23: C5 IN_ORA_RED
  24, 25: A5 IN_BROWN_BROWN_
  26, 27: A4 IN_BROWN_GREY
  28, 29: A1 copy of ACT_BLUE_GREY
  2a, 2b: A3 dangerous to use
  the rest are unused


  LAYOUT | 2[a...f]zz  tiva accessory boards
  
  zz = 00..0B: 5 leds
    00 : red led
    02 : yellow led
    04 : green led
    06 : blue led
    08 : blue-switch-led
    0a : gold-switch-led

  zz = 10..17: 4 relays
    10 : rel1
    12 : rel2
    14 : rel3
    16 : rel4

  zz = 20..2F: 8 outputs
    20 : out0
    22 : out1
    ...
    2e : out7

 */


#define BRACZ_LAYOUT 0x0501010114FF0000ULL
#define BRACZ_SPEEDS (BRACZ_LAYOUT - 0x10000ULL)

extern Board brd;

struct I2CBoard {
  I2CBoard(int a)
      : name_(StringPrintf("B%d", a-0x20)),
        address_(a),
        signal_offset_(0),
        LedRed(
            &brd,
            name_ + "LedRed",
            BRACZ_LAYOUT | (a<<8) | 0x00,
            BRACZ_LAYOUT | (a<<8) | 0x01,
            (a & 0xf), OFS_IOA, 0),
        LedGreen(
            &brd,
            name_ + "LedGreen",
            BRACZ_LAYOUT | (a<<8) | 0x02,
            BRACZ_LAYOUT | (a<<8) | 0x03,
            (a & 0xf), OFS_IOA, 1),
        ActOraRed(
            &brd,
            name_ + "ActOraRed",
            BRACZ_LAYOUT | (a<<8) | 0x10,
            BRACZ_LAYOUT | (a<<8) | 0x11,
            (a & 0xf), OFS_IOA + 1, 0),
        ActOraGreen(
            &brd,
            name_ + "ActOraGreen",
            BRACZ_LAYOUT | (a<<8) | 0x12,
            BRACZ_LAYOUT | (a<<8) | 0x13,
            (a & 0xf), OFS_IOA + 1, 1),
        ActGreenRed(
            &brd,
            name_ + "ActGreenRed",
            BRACZ_LAYOUT | (a<<8) | 0x16,
            BRACZ_LAYOUT | (a<<8) | 0x17,
            (a & 0xf), OFS_IOA + 1, 2),
        ActGreenGreen(
            &brd,
            name_ + "ActGreenGreen",
            BRACZ_LAYOUT | (a<<8) | 0x14,
            BRACZ_LAYOUT | (a<<8) | 0x15,
            (a & 0xf), OFS_IOA + 1, 3),
        ActBlueBrown(
            &brd,
            name_ + "ActBlueBrown",
            BRACZ_LAYOUT | (a<<8) | 0x18,
            BRACZ_LAYOUT | (a<<8) | 0x19,
            (a & 0xf), OFS_IOA + 1, 4),
        ActBlueGrey(
            &brd,
            name_ + "ActBlueGrey",
            BRACZ_LAYOUT | (a<<8) | 0x1a,
            BRACZ_LAYOUT | (a<<8) | 0x1b,
            (a & 0xf), OFS_IOA + 1, 5),

        RelGreen(
            &brd,
            name_ + "RelGreen",
            BRACZ_LAYOUT | (a<<8) | 0x1c,
            BRACZ_LAYOUT | (a<<8) | 0x1d,
            (a & 0xf), OFS_IOA + 1, 6),
        RelBlue(
            &brd,
            name_ + "RelBlue",
            BRACZ_LAYOUT | (a<<8) | 0x1e,
            BRACZ_LAYOUT | (a<<8) | 0x1f,
            (a & 0xf), OFS_IOA + 1, 7),

        InOraRed(
            &brd,
            name_ + "InOraRed",
            BRACZ_LAYOUT | (a<<8) | 0x22,
            BRACZ_LAYOUT | (a<<8) | 0x23,
            (a & 0xf), OFS_IOB, 0),
        InOraGreen(
            &brd,
            name_ + "InOraGreen",
            BRACZ_LAYOUT | (a<<8) | 0x20,
            BRACZ_LAYOUT | (a<<8) | 0x21,
            (a & 0xf), OFS_IOB, 1),
        InBrownGrey(
            &brd,
            name_ + "InBrownGrey",
            BRACZ_LAYOUT | (a<<8) | 0x26,
            BRACZ_LAYOUT | (a<<8) | 0x27,
            (a & 0xf), OFS_IOB, 2),
        InBrownBrown(
            &brd,
            name_ + "InBrownBrown",
            BRACZ_LAYOUT | (a<<8) | 0x24,
            BRACZ_LAYOUT | (a<<8) | 0x25,
            (a & 0xf), OFS_IOB, 3),
        InA3(
            &brd,
            name_ + "InA3",
            BRACZ_LAYOUT | (a<<8) | 0x2a,
            BRACZ_LAYOUT | (a<<8) | 0x2b,
            (a & 0xf), OFS_IOB, 4) {}

  // Allocates a new signal variable off this extender board. Returns the base
  // eventid.
  uint64_t NewSignalVariable() {
    return ((BRACZ_LAYOUT & ~0xffffff) |
            (address_ << 16) |
            ((signal_offset_++ * 2) << 8));
  }

  string name_;
  uint8_t address_;
  // next free signal offset.
  int signal_offset_;

  EventBasedVariable LedRed, LedGreen;

  EventBasedVariable ActOraRed, ActOraGreen;
  EventBasedVariable ActGreenRed, ActGreenGreen;
  EventBasedVariable ActBlueBrown, ActBlueGrey;
  EventBasedVariable RelGreen, RelBlue;

  EventBasedVariable InOraRed, InOraGreen;
  EventBasedVariable InBrownGrey, InBrownBrown;

  EventBasedVariable InA3;
};

struct I2CSignal {
  template<class EXT>
  I2CSignal(EXT* extender, uint8_t signal_id, const string& name)
      : signal(&brd, name, extender->NewSignalVariable(), signal_id) {}

  SignalVariable signal;
};

struct PandaControlBoard {
  PandaControlBoard()
      : name_("panda."),
        l0(
            &brd,
            name_ + "L0",
            BRACZ_LAYOUT | 0x2030,
            BRACZ_LAYOUT | 0x2031,
            7, 31, 4),
        l1(
            &brd,
            name_ + "L1",
            BRACZ_LAYOUT | 0x2032,
            BRACZ_LAYOUT | 0x2033,
            7, 31, 5),
        l2(
            &brd,
            name_ + "L2",
            BRACZ_LAYOUT | 0x2034,
            BRACZ_LAYOUT | 0x2035,
            7, 31, 6),
        l3(
            &brd,
            name_ + "L3",
            BRACZ_LAYOUT | 0x2036,
            BRACZ_LAYOUT | 0x2037,
            7, 31, 7),
        l4(
            &brd,
            name_ + "L4",
            BRACZ_LAYOUT | 0x2038,
            BRACZ_LAYOUT | 0x2039,
            7, 30, 0),
        l5(
            &brd,
            name_ + "L5",
            BRACZ_LAYOUT | 0x203a,
            BRACZ_LAYOUT | 0x203b,
            7, 30, 1)
  {}
  string name_;
  EventBasedVariable l0, l1, l2, l3, l4, l5;
};

struct LPC11C {
  LPC11C()
      : l0(
            &brd,
            "LPC11C.L0",
            BRACZ_LAYOUT | 0x2020,
            BRACZ_LAYOUT | 0x2021,
            7, 30, 2)
  {}

  EventBasedVariable l0;
};

struct NativeIO {
  NativeIO(int a)
      : name_(StringPrintf("N%d", a-0x20)),
        l0(
            &brd,
            name_ + "L0",
            BRACZ_LAYOUT | (a << 8) | 0x20,
            BRACZ_LAYOUT | (a << 8) | 0x21,
            (a & 0xf), OFS_IOA, 7),
        r0(
            &brd,
            name_ + "R0",
            BRACZ_LAYOUT | (a << 8) | 0,
            BRACZ_LAYOUT | (a << 8) | 1,
            (a & 0xf), OFS_IOA, 0),
        r1(
            &brd,
            name_ + "R1",
            BRACZ_LAYOUT | (a << 8) | 2,
            BRACZ_LAYOUT | (a << 8) | 3,
            (a & 0xf), OFS_IOA, 1),
        r2(
            &brd,
            name_ + "R2",
            BRACZ_LAYOUT | (a << 8) | 4,
            BRACZ_LAYOUT | (a << 8) | 5,
            (a & 0xf), OFS_IOA, 2),
        r3(
            &brd,
            name_ + "R3",
            BRACZ_LAYOUT | (a << 8) | 6,
            BRACZ_LAYOUT | (a << 8) | 7,
            (a & 0xf), OFS_IOA, 3),
        d0(
            &brd,
            name_ + "D0",
            BRACZ_LAYOUT | (a << 8) | 0x10,
            BRACZ_LAYOUT | (a << 8) | 0x11,
            (a & 0xf), OFS_IOB, 0),
        d1(
            &brd,
            name_ + "D1",
            BRACZ_LAYOUT | (a << 8) | 0x12,
            BRACZ_LAYOUT | (a << 8) | 0x13,
            (a & 0xf), OFS_IOB, 1),
        d2(
            &brd,
            name_ + "D2",
            BRACZ_LAYOUT | (a << 8) | 0x14,
            BRACZ_LAYOUT | (a << 8) | 0x15,
            (a & 0xf), OFS_IOB, 2),
        d3(
            &brd,
            name_ + "D3",
            BRACZ_LAYOUT | (a << 8) | 0x16,
            BRACZ_LAYOUT | (a << 8) | 0x17,
            (a & 0xf), OFS_IOB, 3)
  {}

  string name_;
  EventBasedVariable l0;
  EventBasedVariable r0, r1, r2, r3;
  EventBasedVariable d0, d1, d2, d3;
};


struct AccBoard {
  AccBoard(int a)
      : name_(StringPrintf("A%d", a-0x2a)),
        address_(a),
        signal_offset_(0),
        LedRed(
            &brd,
            name_ + "LedRed",
            BRACZ_LAYOUT | (a<<8) | 0x00,
            BRACZ_LAYOUT | (a<<8) | 0x01,
            (a & 0xf) - 0xa, OFS_IOA, 0),
        LedYellow(
            &brd,
            name_ + "LedYellow",
            BRACZ_LAYOUT | (a<<8) | 0x02,
            BRACZ_LAYOUT | (a<<8) | 0x03,
            (a & 0xf) - 0xa, OFS_IOA, 1),
        LedGreen(
            &brd,
            name_ + "LedGreen",
            BRACZ_LAYOUT | (a<<8) | 0x04,
            BRACZ_LAYOUT | (a<<8) | 0x05,
            (a & 0xf) - 0xa, OFS_IOA, 2),
        LedBlue(
            &brd,
            name_ + "LedBlue",
            BRACZ_LAYOUT | (a<<8) | 0x06,
            BRACZ_LAYOUT | (a<<8) | 0x07,
            (a & 0xf) - 0xa, OFS_IOA, 3),

        LedBlueSw(
            &brd,
            name_ + "LedBlueSw",
            BRACZ_LAYOUT | (a<<8) | 0x08,
            BRACZ_LAYOUT | (a<<8) | 0x09,
            (a & 0xf) - 0xa, OFS_IOA, 4),
        LedGoldSw(
            &brd,
            name_ + "LedGoldSw",
            BRACZ_LAYOUT | (a<<8) | 0x0a,
            BRACZ_LAYOUT | (a<<8) | 0x0b,
            (a & 0xf) - 0xa, OFS_IOA, 5),

        Rel0(
            &brd,
            name_ + "R0",
            BRACZ_LAYOUT | (a<<8) | 0x10,
            BRACZ_LAYOUT | (a<<8) | 0x11,
            (a & 0xf) - 0xa, OFS_IOA + 1, 0),
        Rel1(
            &brd,
            name_ + "R1",
            BRACZ_LAYOUT | (a<<8) | 0x12,
            BRACZ_LAYOUT | (a<<8) | 0x13,
            (a & 0xf) - 0xa, OFS_IOA + 1, 1),
        Rel2(
            &brd,
            name_ + "R2",
            BRACZ_LAYOUT | (a<<8) | 0x14,
            BRACZ_LAYOUT | (a<<8) | 0x15,
            (a & 0xf) - 0xa, OFS_IOA + 1, 2),
        Rel3(
            &brd,
            name_ + "R3",
            BRACZ_LAYOUT | (a<<8) | 0x16,
            BRACZ_LAYOUT | (a<<8) | 0x17,
            (a & 0xf) - 0xa, OFS_IOA + 1, 3),

        Act0(
            &brd,
            name_ + "Act0",
            BRACZ_LAYOUT | (a<<8) | 0x20,
            BRACZ_LAYOUT | (a<<8) | 0x21,
            (a & 0xf) - 0xa, OFS_IOB, 0),
        Act1(
            &brd,
            name_ + "Act1",
            BRACZ_LAYOUT | (a<<8) | 0x22,
            BRACZ_LAYOUT | (a<<8) | 0x23,
            (a & 0xf) - 0xa, OFS_IOB, 1),
        Act2(
            &brd,
            name_ + "Act2",
            BRACZ_LAYOUT | (a<<8) | 0x24,
            BRACZ_LAYOUT | (a<<8) | 0x25,
            (a & 0xf) - 0xa, OFS_IOB, 2),
        Act3(
            &brd,
            name_ + "Act3",
            BRACZ_LAYOUT | (a<<8) | 0x26,
            BRACZ_LAYOUT | (a<<8) | 0x27,
            (a & 0xf) - 0xa, OFS_IOB, 3),
        Act4(
            &brd,
            name_ + "Act4",
            BRACZ_LAYOUT | (a<<8) | 0x28,
            BRACZ_LAYOUT | (a<<8) | 0x29,
            (a & 0xf) - 0xa, OFS_IOB, 4),
        Act5(
            &brd,
            name_ + "Act5",
            BRACZ_LAYOUT | (a<<8) | 0x2a,
            BRACZ_LAYOUT | (a<<8) | 0x2b,
            (a & 0xf) - 0xa, OFS_IOB, 5),
        Act6(
            &brd,
            name_ + "Act6",
            BRACZ_LAYOUT | (a<<8) | 0x2c,
            BRACZ_LAYOUT | (a<<8) | 0x2d,
            (a & 0xf) - 0xa, OFS_IOB, 6),
        Act7(
            &brd,
            name_ + "Act7",
            BRACZ_LAYOUT | (a<<8) | 0x2e,
            BRACZ_LAYOUT | (a<<8) | 0x2f,
            (a & 0xf) - 0xa, OFS_IOB, 7),

        In0(
            &brd,
            name_ + "In0",
            BRACZ_LAYOUT | (a<<8) | 0x30,
            BRACZ_LAYOUT | (a<<8) | 0x31,
            (a & 0xf) - 0xa, OFS_IOB + 1, 0),
        In1(
            &brd,
            name_ + "In1",
            BRACZ_LAYOUT | (a<<8) | 0x32,
            BRACZ_LAYOUT | (a<<8) | 0x33,
            (a & 0xf) - 0xa, OFS_IOB + 1, 1),
        In2(
            &brd,
            name_ + "In2",
            BRACZ_LAYOUT | (a<<8) | 0x34,
            BRACZ_LAYOUT | (a<<8) | 0x35,
            (a & 0xf) - 0xa, OFS_IOB + 1, 2),
        In3(
            &brd,
            name_ + "In3",
            BRACZ_LAYOUT | (a<<8) | 0x36,
            BRACZ_LAYOUT | (a<<8) | 0x37,
            (a & 0xf) - 0xa, OFS_IOB + 1, 3),
        In4(
            &brd,
            name_ + "In4",
            BRACZ_LAYOUT | (a<<8) | 0x38,
            BRACZ_LAYOUT | (a<<8) | 0x39,
            (a & 0xf) - 0xa, OFS_IOB + 1, 4),
        In5(
            &brd,
            name_ + "In5",
            BRACZ_LAYOUT | (a<<8) | 0x3a,
            BRACZ_LAYOUT | (a<<8) | 0x3b,
            (a & 0xf) - 0xa, OFS_IOB + 1, 5),
        In6(
            &brd,
            name_ + "In6",
            BRACZ_LAYOUT | (a<<8) | 0x3c,
            BRACZ_LAYOUT | (a<<8) | 0x3d,
            (a & 0xf) - 0xa, OFS_IOB + 1, 6),
        In7(
            &brd,
            name_ + "In7",
            BRACZ_LAYOUT | (a<<8) | 0x3e,
            BRACZ_LAYOUT | (a<<8) | 0x3f,
            (a & 0xf) - 0xa, OFS_IOB + 1, 7),
        ActGreenGreen(Act0),
        ActGreenRed(Act1),
        ActOraRed(Act2),
        ActOraGreen(Act3),
        ActBlueBrown(Act4),
        ActBlueGrey(Act5),
        InBrownBrown(In0),
        InBrownGrey(In1),
        InOraRed(In2),
        InOraGreen(In3),
        InGreenGreen(In4),
        InGreenYellow(In5)
  {}


  // Allocates a new signal variable off this extender board. Returns the base
  // eventid.
  uint64_t NewSignalVariable() {
    return ((BRACZ_LAYOUT & ~0xffffff) |
            (address_ << 16) |
            ((signal_offset_++ * 2) << 8));
  }

  string name_;
  uint8_t address_;
  // next free signal offset.
  int signal_offset_;

  EventBasedVariable LedRed, LedYellow, LedGreen, LedBlue, LedBlueSw, LedGoldSw;
  EventBasedVariable Rel0, Rel1, Rel2, Rel3;
  EventBasedVariable Act0, Act1, Act2, Act3, Act4, Act5, Act6, Act7;
  EventBasedVariable In0, In1, In2, In3, In4, In5, In6, In7;

  EventBasedVariable &ActGreenGreen, &ActGreenRed, &ActOraRed, &ActOraGreen,
      &ActBlueBrown, &ActBlueGrey;

  EventBasedVariable &InBrownBrown, &InBrownGrey, &InOraRed, &InOraGreen,
      &InGreenGreen, &InGreenYellow;
};


extern EventBasedVariable is_paused;



#endif // _AUTOMATA_BRACZ_LAYOUT_HXX_
