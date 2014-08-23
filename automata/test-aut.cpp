#include <stdio.h>
#include <unistd.h>

#include <string>
#include <fstream>
#include <memory>
using namespace std;

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"

#include "bracz-layout.hxx"
#include "control-logic.hxx"

//#define OFS_GLOBAL_BITS 24

using namespace automata;




Board brd;

EventBasedVariable is_paused(&brd,
                             "is_paused",
                             BRACZ_LAYOUT | 0x0000,
                             BRACZ_LAYOUT | 0x0001,
                             7, 31, 3);


I2CBoard b5(0x25), b6(0x26), b1(0x21), b3(0x23);


StateRef StGreen(2);
StateRef StGoing(3);


StateRef StUser1(10);
StateRef StUser2(11);

PandaControlBoard panda_bridge;

LPC11C lpc11_back;

PhysicalSignal S201(&b5.InBrownBrown, &b5.RelGreen, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr);
PhysicalSignal S382(&b6.InBrownGrey, &b6.RelGreen, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr);

PhysicalSignal S401(&b3.InBrownBrown, &b3.RelGreen, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr);
PhysicalSignal S501(&b1.InBrownGrey, &b1.RelGreen, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr);

int next_temp_bit = 0;
GlobalVariable* NewTempVariable(Board* board) {
  int counter = next_temp_bit++;
  int client, offset, bit;
  if (!DecodeOffset(counter, &client, &offset, &bit)) {
    fprintf(stderr, "Too many local variables. Cannot allocate more.");
    abort();
  }
  return new EventBasedVariable(
      board,
      StringPrintf("tmp_var_%d", counter),
      BRACZ_LAYOUT | 0x3000 | (counter << 1),
      BRACZ_LAYOUT | 0x3000 | (counter << 1) | 1,
      client, offset, bit);
}


class StrategyAutomata : public Automata {
 public:
  StrategyAutomata(const string& name) : Automata(name) {}

 protected:
  static const int kTimeTakenToGoBusy = 2;
  static const int kTimeTakenToGoFree = 3;

  void SensorDebounce(const LocalVariable& raw, LocalVariable* filtered) {
    if (!sensor_tmp_var_.get()) {
      sensor_tmp_var_.reset(NewTempVariable(board()));
    }
    LocalVariable* last(ImportVariable(sensor_tmp_var_.get()));
    // If there is a flip, we start a timer. The timer length depends on the
    // edge.
    Def().IfReg0(raw).IfReg1(*last).ActTimer(kTimeTakenToGoBusy);
    Def().IfReg1(raw).IfReg0(*last).ActTimer(kTimeTakenToGoFree);
    DefCopy(raw, last);
    // If no flip happened for the timer length, we put the value into the
    // filtered.
    Def().IfTimerDone().IfReg1(*last).ActReg0(filtered);
    Def().IfTimerDone().IfReg0(*last).ActReg1(filtered);
  }

  void Strategy(LocalVariable& src_busy, LocalVariable* src_go,
                LocalVariable& dst_busy) {
    Def().IfState(StInit).ActState(StBase).ActReg0(src_go);

    Def().IfState(StBase).IfReg1(ImportVariable(is_paused))
        .IfReg1(src_busy).IfReg0(dst_busy)
        .ActState(StGreen);

    Def().IfState(StGreen).ActReg1(src_go);

    Def().IfState(StGreen).IfReg0(src_busy)
        .ActReg0(src_go).ActState(StGoing);

    Def().IfState(StGoing).IfReg1(dst_busy).ActState(StBase);
  }

  unique_ptr<GlobalVariable> sensor_tmp_var_;
};

unique_ptr<GlobalVariable> blink_variable(NewTempVariable(&brd));

EventBasedVariable led(&brd,
                       "led",
                       0x0502010202650012ULL,
                       0x0502010202650013ULL,
                       7, 31, 1);

DefAut(blinkaut, brd, {
    const int kBlinkSpeed = 3;
    LocalVariable* rep(ImportVariable(blink_variable.get()));
    Def().IfState(StInit).ActState(StUser1);
    Def().IfState(StUser1).IfTimerDone()
        .ActTimer(kBlinkSpeed).ActState(StUser2).ActReg0(rep);
    Def().IfState(StUser2).IfTimerDone()
        .ActTimer(kBlinkSpeed).ActState(StUser1).ActReg1(rep);

    DefCopy(*rep, ImportVariable(&b1.LedRed));
    DefCopy(*rep, ImportVariable(&b3.LedRed));
    DefCopy(*rep, ImportVariable(&b5.LedRed));
    DefCopy(*rep, ImportVariable(&b6.LedRed));
    DefCopy(*rep, ImportVariable(&panda_bridge.l4));
    DefCopy(*rep, ImportVariable(&lpc11_back.l0));
    });

DefAut(testaut, brd, {
    Def().IfState(StInit).ActState(StBase);
    });

DefAut(auto_green, brd, {
    DefNCopy(ImportVariable(*S201.sensor_raw),
            ImportVariable(S201.signal_raw));
    DefNCopy(ImportVariable(*S382.sensor_raw),
            ImportVariable(S382.signal_raw));
    DefNCopy(ImportVariable(*S501.sensor_raw),
            ImportVariable(S501.signal_raw));
  });


DefCustomAut(magictest, brd, StrategyAutomata, {
    SensorDebounce(ImportVariable(*S501.sensor_raw),
                   ImportVariable(&b1.InA3));
    /*Strategy(ImportVariable(&b1.InA3),
             ImportVariable(&b1.InOraRed),
             ImportVariable(&b1.RelBlue));*/
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
