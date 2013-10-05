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

 */


#define BRACZ_LAYOUT 0x0501010114FF0000ULL

extern Board brd;

struct I2CBoard {
  I2CBoard(int a)
      : LedRed(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x00,
            BRACZ_LAYOUT | (a<<8) | 0x01,
            a & 0xf, OFS_IOA, 0),
        LedGreen(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x02,
            BRACZ_LAYOUT | (a<<8) | 0x03,
            a & 0xf, OFS_IOA, 1),
        ActOraRed(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x10,
            BRACZ_LAYOUT | (a<<8) | 0x11,
            a & 0xf, OFS_IOA + 1, 0),
        ActOraGreen(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x12,
            BRACZ_LAYOUT | (a<<8) | 0x13,
            a & 0xf, OFS_IOA + 1, 1),
        ActGreenRed(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x16,
            BRACZ_LAYOUT | (a<<8) | 0x17,
            a & 0xf, OFS_IOA + 1, 2),
        ActGreenGreen(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x14,
            BRACZ_LAYOUT | (a<<8) | 0x15,
            a & 0xf, OFS_IOA + 1, 3),
        ActBlueBrown(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x18,
            BRACZ_LAYOUT | (a<<8) | 0x19,
            a & 0xf, OFS_IOA + 1, 4),
        ActBlueGrey(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x1a,
            BRACZ_LAYOUT | (a<<8) | 0x1b,
            a & 0xf, OFS_IOA + 1, 5),

        RelGreen(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x1c,
            BRACZ_LAYOUT | (a<<8) | 0x1d,
            a & 0xf, OFS_IOA + 1, 6),
        RelBlue(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x1e,
            BRACZ_LAYOUT | (a<<8) | 0x1f,
            a & 0xf, OFS_IOA + 1, 7),

        InOraRed(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x22,
            BRACZ_LAYOUT | (a<<8) | 0x23,
            a & 0xf, OFS_IOB, 0),
        InOraGreen(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x20,
            BRACZ_LAYOUT | (a<<8) | 0x21,
            a & 0xf, OFS_IOB, 1),
        InBrownGrey(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x26,
            BRACZ_LAYOUT | (a<<8) | 0x27,
            a & 0xf, OFS_IOB, 2),
        InBrownBrown(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x24,
            BRACZ_LAYOUT | (a<<8) | 0x25,
            a & 0xf, OFS_IOB, 3),
        InA3(
            &brd,
            BRACZ_LAYOUT | (a<<8) | 0x2a,
            BRACZ_LAYOUT | (a<<8) | 0x2b,
            a & 0xf, OFS_IOB, 4) {}

  EventBasedVariable LedRed, LedGreen;

  EventBasedVariable ActOraRed, ActOraGreen;
  EventBasedVariable ActGreenRed, ActGreenGreen;
  EventBasedVariable ActBlueBrown, ActBlueGrey;
  EventBasedVariable RelGreen, RelBlue;

  EventBasedVariable InOraRed, InOraGreen;
  EventBasedVariable InBrownGrey, InBrownBrown;

  EventBasedVariable InA3;
};


struct PandaControlBoard {
  PandaControlBoard()
      : l0(
            &brd,
            BRACZ_LAYOUT | 0x2030,
            BRACZ_LAYOUT | 0x2031,
            7, 31, 4),
        l1(
            &brd,
            BRACZ_LAYOUT | 0x2032,
            BRACZ_LAYOUT | 0x2033,
            7, 31, 5),
        l2(
            &brd,
            BRACZ_LAYOUT | 0x2034,
            BRACZ_LAYOUT | 0x2035,
            7, 31, 6),
        l3(
            &brd,
            BRACZ_LAYOUT | 0x2036,
            BRACZ_LAYOUT | 0x2037,
            7, 31, 7),
        l4(
            &brd,
            BRACZ_LAYOUT | 0x2038,
            BRACZ_LAYOUT | 0x2039,
            7, 30, 0),
        l5(
            &brd,
            BRACZ_LAYOUT | 0x203a,
            BRACZ_LAYOUT | 0x203b,
            7, 30, 1)
  {}

  EventBasedVariable l0, l1, l2, l3, l4, l5;
};

struct LPC11C {
  LPC11C()
      : l0(
            &brd,
            BRACZ_LAYOUT | 0x2020,
            BRACZ_LAYOUT | 0x2021,
            7, 30, 2)
  {}

  EventBasedVariable l0;
};

extern EventBasedVariable is_paused;



#endif // _AUTOMATA_BRACZ_LAYOUT_HXX_
