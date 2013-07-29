#include <stdio.h>
#include <unistd.h>

#include <string>
#include <fstream>
using namespace std;

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"


#include "../cs/src/base.h"
//#define OFS_GLOBAL_BITS 24

using namespace automata;

#define BRACZ_LAYOUT 0x0501010114FF0000ULL

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


Board brd;


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

I2CBoard b5(0x25), b6(0x26), b1(0x21), b3(0x23);

StateRef StateInit(0);
StateRef StateBase(1);

StateRef StateUser1(2);
StateRef StateUser2(3);


EventBasedVariable led(&brd,
                       0x0502010202650012ULL,
                       0x0502010202650013ULL,
                       0, OFS_GLOBAL_BITS, 1);

EventBasedVariable intev(&brd,
                         0x0502010202650022ULL,
                         0x0502010202650023ULL,
                         0, OFS_GLOBAL_BITS, 3);

EventBasedVariable repev(&brd,
                         0x0502010202650032ULL,
                         0x0502010202650033ULL,
                         0, OFS_GLOBAL_BITS, 4);

EventBasedVariable vled1(&brd,
                         0x0502010202650040ULL,
                         0x0502010202650041ULL,
                         1, OFS_IOA, 1);
EventBasedVariable vled2(&brd,
                         0x0502010202650042ULL,
                         0x0502010202650043ULL,
                         1, OFS_IOA, 0);
EventBasedVariable vled3(&brd,
                         0x0502010202650060ULL,
                         0x0502010202650061ULL,
                         2, OFS_IOA, 1);
EventBasedVariable vled4(&brd,
                         0x0502010202650062ULL,
                         0x0502010202650063ULL,
                         2, OFS_IOA, 0);

EventBasedVariable inpb1(&brd,
                         0x0502010202650082ULL,
                         0x0502010202650083ULL,
                         1, OFS_IOB, 2);

EventBasedVariable inpb2(&brd,
                         0x0502010202650084ULL,
                         0x0502010202650085ULL,
                         2, OFS_IOB, 2);


DefAut(testaut, brd, {
        Def().IfState(StateInit).ActState(StateBase);
    });


/*DefAut(blinker, brd, {
        LocalVariable& repvar(ImportVariable(&led));
        LocalVariable& l1(ImportVariable(&b5.LedRed));
        LocalVariable& l2(ImportVariable(&b6.LedGreen));
        Def().IfState(StateInit).ActState(StateUser1);
        Def().IfState(StateUser1).IfTimerDone().ActTimer(1).ActState(StateUser2);
        Def().IfState(StateUser2).IfTimerDone().ActTimer(1).ActState(StateUser1);
        Def().IfState(StateUser1).ActReg1(repvar);
        Def().IfState(StateUser2).ActReg0(repvar);

        Def().IfState(StateUser1).ActReg1(l1);
        Def().IfState(StateUser2).ActReg0(l1);

        Def().IfState(StateUser2).ActReg1(l2);
        Def().IfState(StateUser1).ActReg0(l2);
        });*/

DefAut(copier, brd, {
        LocalVariable& ledvar(ImportVariable(&led));
        LocalVariable& intvar(ImportVariable(&intev));
        Def().IfReg1(ledvar).ActReg1(intvar);
        Def().IfReg0(ledvar).ActReg0(intvar);
    });

DefAut(xcopier, brd, {
        LocalVariable& ledvar(ImportVariable(&vled4));
        LocalVariable& btnvar(ImportVariable(&inpb1));
        DefCopy(ImportVariable(&b5.InBrownBrown), ImportVariable(&b5.LedRed));
        //DefCopy(btnvar, ledvar);
    });

DefAut(xcopier2, brd, {
        LocalVariable& ledvar(ImportVariable(&vled2));
        LocalVariable& btnvar(ImportVariable(&inpb2));
        //DefNCopy(btnvar, ledvar);
    });


int main(int argc, char** argv) {
    FILE* f = fopen("automata.bin", "wb");
    assert(f);
    string output;
    brd.Render(&output);
    fwrite(output.data(), 1, output.size(), f);
    fclose(f);
    return 0;
};
