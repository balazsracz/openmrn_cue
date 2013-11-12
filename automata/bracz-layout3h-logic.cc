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
                             BRACZ_LAYOUT | 0x0000,
                             BRACZ_LAYOUT | 0x0001,
                             7, 31, 3);


I2CBoard b5(0x25), b6(0x26), b1(0x21), b3(0x23);


/*StateRef StGreen(2);
StateRef StGoing(3);
*/

StateRef StUser1(10);
StateRef StUser2(11);

PandaControlBoard panda_bridge;

LPC11C lpc11_back;


PhysicalSignal WWB14(&b5.InBrownBrown, &b5.RelGreen);
PhysicalSignal A301(&b6.InBrownGrey, &b6.RelGreen);

PhysicalSignal YYC23(&b3.InBrownBrown, &b3.RelGreen);
PhysicalSignal XXB1(&b1.InBrownGrey, &b1.RelGreen);


int next_temp_bit = 480;
GlobalVariable* NewTempVariable(Board* board) {
  int counter = next_temp_bit++;
  int client, offset, bit;
  if (!DecodeOffset(counter, &client, &offset, &bit)) {
    fprintf(stderr, "Too many local variables. Cannot allocate more.");
    abort();
  }
  return new EventBasedVariable(
      board,
      BRACZ_LAYOUT | 0x3000 | (counter << 1),
      BRACZ_LAYOUT | 0x3000 | (counter << 1) | 1,
      client, offset, bit);
}

unique_ptr<GlobalVariable> blink_variable(NewTempVariable(&brd));

EventBasedVariable led(&brd,
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

class StandardBlock : public StraightTrackInterface {
 public:
  StandardBlock(Board* brd, PhysicalSignal* physical, const EventBlock::Allocator& alloc)
      : request_green_(alloc.Allocate("request_green")),
        body_(EventBlock::Allocator(&alloc, "body", 24, 8)),
        body_det_(EventBlock::Allocator(&alloc, "body_det", 24, 8),
                  physical->sensor_raw),
        signal_(EventBlock::Allocator(&alloc, "body_det", 24, 8), request_green_.get(),
                physical->signal_raw),
        physical_(physical),
        aut_body_(brd, &body_),
        aut_body_det_(brd, &body_det_),
        aut_signal_(brd, &signal_) {
    BindSequence({&body_, &body_det_, &signal_});
  }

  virtual CtrlTrackInterface* side_a() { return body_.side_a(); }
  virtual CtrlTrackInterface* side_b() { return signal_.side_b(); }

  GlobalVariable* request_green() { return request_green_.get(); }
  const GlobalVariable& route() { return *body_det_.route_set_ab_; }
  const GlobalVariable& detector() { return *body_det_.simulated_occupancy_; }

 private:
  unique_ptr<GlobalVariable> request_green_;

 public:
  StraightTrackLong body_;
  StraightTrackWithRawDetector body_det_;
  SignalPiece signal_;

 private:
  PhysicalSignal* physical_;

  StandardPluginAutomata aut_body_;
  StandardPluginAutomata aut_body_det_;
  StandardPluginAutomata aut_signal_;
};

EventBlock logic(&brd, BRACZ_LAYOUT | 0xE000, "logic");

StandardBlock Block_WWB14(&brd, &WWB14,
                          *logic.allocator());
StandardBlock Block_A301(&brd, &A301,
                         *logic.allocator());
//StandardBlock Block_YYC23(&brd, &YYC23,
//                          BRACZ_LAYOUT | 0x3900 | 0, 48*2);
StandardBlock Block_XXB1(&brd, &XXB1,
                         *logic.allocator());

#define BLOCK_SEQUENCE   &Block_XXB1, &Block_A301,  &Block_WWB14,  /*&Block_YYC23,*/  &Block_XXB1


std::vector<StandardBlock*> block_sequence = { BLOCK_SEQUENCE };

bool ignored1 = BindSequence({BLOCK_SEQUENCE});

DefAut(control_logic, brd, {
    for (size_t i = 0; i < block_sequence.size() - 1; ++i) {
      StandardBlock* current = block_sequence[i];
      StandardBlock* next = block_sequence[i+1];
      Def()
          .IfReg1(ImportVariable(current->detector()))
          .IfReg0(ImportVariable(next->route()))
          .IfReg0(ImportVariable(next->detector()))
          .ActReg1(ImportVariable(current->request_green()));
    }
  });


/*DefAut(auto_green, brd, {
    DefNCopy(ImportVariable(*S201.sensor_raw),
            ImportVariable(S201.signal_raw));
    DefNCopy(ImportVariable(*S382.sensor_raw),
            ImportVariable(S382.signal_raw));
    DefNCopy(ImportVariable(*S501.sensor_raw),
            ImportVariable(S501.signal_raw));
            });*/


/*DefCustomAut(magictest, brd, StrategyAutomata, {
    SensorDebounce(ImportVariable(*S501.sensor_raw),
                   ImportVariable(&b1.InA3));
    // Strategy(ImportVariable(&b1.InA3),
    //          ImportVariable(&b1.InOraRed),
    //          ImportVariable(&b1.RelBlue));
    });*/

/*Defaut(blinker, brd, {
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

    f = fopen("variables.txt", "w");
    assert(f);
    map<int, string>& m(*automata::GetOffsetMap());
    for (const auto& it : m) {
      fprintf(f, "%04x: %s\n", it.first * 2, it.second.c_str());
    }
    fclose(f);

    return 0;
};
