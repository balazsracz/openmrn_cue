#include <stdio.h>
#include <unistd.h>

#include <string>
#include <fstream>
#include <memory>
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

StateRef StInit(0);
StateRef StBase(1);


StateRef StGreen(2);
StateRef StGoing(3);


StateRef StUser1(10);
StateRef StUser2(11);


EventBasedVariable is_paused(&brd,
                             BRACZ_LAYOUT | 0x0000 | 0,
                             BRACZ_LAYOUT | 0x0000 | 1,
                             0, OFS_GLOBAL_BITS, 1);




int next_temp_bit = 0;
GlobalVariable* NewTempVariable(Board* board) {
  int counter = next_temp_bit++;
  int bit = counter & 7;
  int autid = counter >> 3;
  int client = autid >> 3;
  int autofs = autid & 7;
  int byteofs = autofs * LEN_AUTOMATA + OFS_BITS;
  if (client >= 8) {
    fprintf(stderr, "Too many local variables. Cannot allocate more.");
    abort();
  }
  return new EventBasedVariable(
      board,
      BRACZ_LAYOUT | 0x3000 | (counter << 1),
      BRACZ_LAYOUT | 0x3000 | (counter << 1) | 1,
      client, byteofs, bit);
}


class StrategyAutomata : public Automata {
 public:
  StrategyAutomata() {}

 protected:
  static const int kTimeTakenToGoBusy = 1;
  static const int kTimeTakenToGoFree = 3;

  void SensorDebounce(LocalVariable& raw, LocalVariable& filtered) {
    if (!sensor_tmp_var_.get()) {
      sensor_tmp_var_.reset(NewTempVariable(board()));
    }
    LocalVariable& last(ImportVariable(sensor_tmp_var_.get()));
    // If there is a flip, we start a timer. The timer length depends on the
    // edge.
    Def().IfReg0(raw).IfReg1(last).ActTimer(kTimeTakenToGoBusy);
    Def().IfReg1(raw).IfReg0(last).ActTimer(kTimeTakenToGoFree);
    DefCopy(raw, last);
    // If no flip happened for the timer length, we put the value into the
    // filtered.
    Def().IfTimerDone().IfReg1(last).ActReg0(filtered);
    Def().IfTimerDone().IfReg0(last).ActReg1(filtered);
  }

  void Strategy(LocalVariable& src_busy, LocalVariable& src_go,
                LocalVariable& dst_busy) {
    Def().IfState(StInit).ActState(StBase).ActReg0(src_go);

    Def().IfState(StBase).IfReg1(ImportVariable(&is_paused))
        .IfReg1(src_busy).IfReg0(dst_busy)
        .ActState(StGreen);

    Def().IfState(StGreen).ActReg1(src_go);

    Def().IfState(StGreen).IfReg0(src_busy)
        .ActReg0(src_go).ActState(StGoing);

    Def().IfState(StGoing).IfReg1(dst_busy).ActState(StBase);
  }




  unique_ptr<GlobalVariable> sensor_tmp_var_;
};



EventBasedVariable led(&brd,
                       0x0502010202650012ULL,
                       0x0502010202650013ULL,
                       7, 31, 1);

DefAut(testaut, brd, {
        Def().IfState(StInit).ActState(StBase);
    });


DefCustomAut(magictest, brd, StrategyAutomata, {
    SensorDebounce(ImportVariable(&b1.InBrownBrown),
                   ImportVariable(&b1.InA3));
    Strategy(ImportVariable(&b1.InA3),
             ImportVariable(&b1.InOraRed),
             ImportVariable(&b1.RelBlue));
  });

/*DefAut(blinker, brd, {
        LocalVariable& repvar(ImportVariable(&led));
        LocalVariable& l1(ImportVariable(&b5.LedRed));
        LocalVariable& l2(ImportVariable(&b6.LedGreen));
        Def().IfState(StInit).ActState(StUser1);
        Def().IfState(StUser1).IfTimerDone().ActTimer(1).ActState(StUser2);
        Def().IfState(StUser2).IfTimerDone().ActTimer(1).ActState(StUser1);
        Def().IfState(StUser1).ActReg1(repvar);
        Def().IfState(StUser2).ActReg0(repvar);

        Def().IfState(StUser1).ActReg1(l1);
        Def().IfState(StUser2).ActReg0(l1);

        Def().IfState(StUser2).ActReg1(l2);
        Def().IfState(StUser1).ActReg0(l2);
        });*/

/*DefAut(copier, brd, {
        LocalVariable& ledvar(ImportVariable(&led));
        LocalVariable& intvar(ImportVariable(&intev));
        Def().IfReg1(ledvar).ActReg1(intvar);
        Def().IfReg0(ledvar).ActReg0(intvar);
    });
*/
/*DefAut(xcopier, brd, {
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

*/


int main(int argc, char** argv) {
    FILE* f = fopen("automata.bin", "wb");
    assert(f);
    string output;
    brd.Render(&output);
    fwrite(output.data(), 1, output.size(), f);
    fclose(f);
    return 0;
};
