#include <stdio.h>
#include <unistd.h>

#include <string>
#include <fstream>
#include <memory>
using namespace std;

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"
#include "registry.hxx"

#include "bracz-layout.hxx"
#include "control-logic.hxx"

//#define OFS_GLOBAL_BITS 24

using namespace automata;

Board brd;

EventBasedVariable is_paused(&brd, "is_paused", BRACZ_LAYOUT | 0x0000,
                             BRACZ_LAYOUT | 0x0001, 7, 31, 3);

EventBasedVariable blink_off(&brd, "blink_off", BRACZ_LAYOUT | 0x0002,
                             BRACZ_LAYOUT | 0x0003, 7, 31, 2);

EventBasedVariable power_acc(&brd, "power_accessories", BRACZ_LAYOUT | 0x0004,
                             BRACZ_LAYOUT | 0x0005, 7, 31, 1);

EventBasedVariable short_det(&brd, "short_detected", BRACZ_LAYOUT | 0x0006,
                             BRACZ_LAYOUT | 0x0007, 7, 31, 0);

EventBasedVariable overcur(&brd, "overcurrent_detected", BRACZ_LAYOUT | 0x0008,
                           BRACZ_LAYOUT | 0x0009, 7, 30, 7);

EventBasedVariable sendmeasure(&brd, "send_current_measurements",
                               BRACZ_LAYOUT | 0x000a, BRACZ_LAYOUT | 0x000b, 7,
                               30, 6);

EventBasedVariable watchdog(&brd, "reset_watchdog",
                            BRACZ_LAYOUT | 0x0010, BRACZ_LAYOUT | 0x0010, 7,
                            30, 5);

EventBasedVariable reset_all_routes(&brd, "reset_routes",
                            BRACZ_LAYOUT | 0x0012, BRACZ_LAYOUT | 0x0013, 7,
                            30, 4);


AccBoard ba(0x2a);
AccBoard bb(0x2b);
I2CBoard b3(0x23);

const GlobalVariable* ctc = &bb.In7;

StateRef StUser1(10);
StateRef StUser2(11);

PandaControlBoard panda_bridge;

I2CSignal signal_XXB2_main(&b3, 8, "XX.B2.main");
I2CSignal signal_XXB2_adv(&b3, 9, "XX.B2.adv");

I2CSignal signal_YYC23_main(&b3, 26, "YY.C23.main");
I2CSignal signal_YYC23_adv(&b3, 27, "YY.C23.adv");

EventBlock logic(&brd, BRACZ_LAYOUT | 0xE000, "logic");

struct ConventionBlock {
 public:
  ConventionBlock(const char* name, GlobalVariable* sensor, GlobalVariable* _relay,
                  GlobalVariable* signal_red, GlobalVariable* signal_green,
                  GlobalVariable* _button)
      : sensor_raw(sensor)
      , relay(_relay)
      , red(signal_red)
      , green(signal_green)
      , button(_button)
      , raw_block(sensor_raw, relay, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
      , block(&brd, &raw_block, EventBlock::Allocator(logic.allocator(), name, 80)) {}

  GlobalVariable* sensor_raw;
  GlobalVariable* relay;
  GlobalVariable* red;
  GlobalVariable* green;
  GlobalVariable* button;
  PhysicalSignal raw_block;
  StandardBlock block;
};


ConventionBlock St1("St1", &ba.In1, &ba.Rel2, &bb.Act4, &bb.Act3, &ba.In5);
ConventionBlock St2("St2", &ba.In0, &ba.Rel3, &bb.Act1, &bb.Act2, &ba.In4);

ConventionBlock Rd1("Rd1", &bb.In2, &bb.Rel0, &bb.Act0, &ba.Act3, &ba.In6);
ConventionBlock Rd2("Rd2", &bb.In1, &bb.Rel1, &ba.Act7, &ba.Act6, &ba.In2);
ConventionBlock Rd3("Rd3", &bb.In0, &bb.Rel2, &ba.Act4, &ba.Act5, &ba.In3);



int next_temp_bit = 480;
GlobalVariable* NewTempVariable(Board* board) {
  int counter = next_temp_bit++;
  int client, offset, bit;
  if (!DecodeOffset(counter, &client, &offset, &bit)) {
    fprintf(stderr, "Too many local variables. Cannot allocate more.");
    abort();
  }
  return new EventBasedVariable(board, StringPrintf("tmp_var_%d", counter),
                                BRACZ_LAYOUT | 0x3000 | (counter << 1),
                                BRACZ_LAYOUT | 0x3000 | (counter << 1) | 1,
                                client, offset, bit);
}

unique_ptr<GlobalVariable> blink_variable(NewTempVariable(&brd));

EventBasedVariable led(&brd, "led", 0x0502010202650012ULL,
                       0x0502010202650013ULL, 7, 31, 1);

class MagnetPause {
 public:
  MagnetPause(MagnetCommandAutomata* aut, GlobalVariable* power_magnets)
      : power_magnets_(power_magnets) {
    aut->AddAutomataPlugin(1, NewCallbackPtr(this, &MagnetPause::PauseImpl));
  }

 private:
  void PauseImpl(Automata* aut) {
    StateRef StPaused(3);
    Automata::LocalVariable* power_acc(aut->ImportVariable(power_magnets_));
    Def().IfState(StInit).ActReg1(power_acc);
    Def().IfReg0(*power_acc).ActState(StPaused);
    Def()
        .IfReg1(*power_acc)
        .IfState(StPaused)
        .ActState(StBase);
  }

  GlobalVariable* power_magnets_;
};

DefAut(watchdog, brd, {
    LocalVariable* w = ImportVariable(&watchdog);
    Def().IfTimerDone().ActReg1(w).ActReg0(w).ActTimer(1);
  });

DefAut(blinkaut, brd, {
  const int kBlinkSpeed = 3;
  LocalVariable* rep(ImportVariable(blink_variable.get()));
  const LocalVariable& lblink_off(ImportVariable(blink_off));
  Def().IfState(StInit).ActState(StUser1);
  Def()
      .IfState(StUser1)
      .IfTimerDone()
      .IfReg0(lblink_off)
      .ActTimer(kBlinkSpeed)
      .ActState(StUser2)
      .ActReg0(rep);
  Def()
      .IfState(StUser2)
      .IfTimerDone()
      .IfReg0(lblink_off)
      .ActTimer(kBlinkSpeed)
      .ActState(StUser1)
      .ActReg1(rep);

  DefCopy(*rep, ImportVariable(&ba.LedBlueSw));
  DefCopy(*rep, ImportVariable(&bb.LedGoldSw));
  DefCopy(*rep, ImportVariable(&b3.LedRed));
  DefCopy(*rep, ImportVariable(&panda_bridge.l4));
});

DefAut(testaut, brd, { Def().IfState(StInit).ActState(StBase); });

// Adds the necessary conditions that represent if there is a train at the
// source track waiting to depart.
void IfSourceTrackReady(StandardBlock* track, Automata::Op* op) {
  op->IfReg0(op->parent()->ImportVariable(is_paused))
      .IfReg0(op->parent()->ImportVariable(reset_all_routes))
      .IfReg1(op->parent()->ImportVariable(track->detector()))
      .IfReg0(op->parent()->ImportVariable(track->route_out()));
}

// Adds the necessary conditions that represent if there is a train at the
// source track waiting to depart.
void IfSourceTrackWithRoute(StandardBlock* track, Automata::Op* op) {
  op->IfReg0(op->parent()->ImportVariable(is_paused))
      .IfReg1(op->parent()->ImportVariable(track->route_in()));
}

// Adds the necessary conditions that represent if the destination track is
// ready to receive a train.
void IfDstTrackReady(StandardBlock* track, Automata::Op* op) {
  op->IfReg0(op->parent()->ImportVariable(track->route_in()))
      .IfReg0(op->parent()->ImportVariable(track->detector()));
}

void SimpleFollowStrategy(
    StandardBlock* src, StandardBlock* dst,
    const std::initializer_list<const GlobalVariable*>& route_points,
    Automata* aut) {
  auto src_cb = NewCallback(&IfSourceTrackReady, src);
  auto src_alt_cb = NewCallback(&IfSourceTrackWithRoute, src);
  auto dst_cb = NewCallback(&IfDstTrackReady, dst);
  Def()
      .RunCallback(&src_cb)
      .Rept(&Automata::Op::IfReg0, route_points)
      .RunCallback(&dst_cb)
      .ActReg1(aut->ImportVariable(src->request_green()));
  // Aggressive set-to-green.
  /*
  Def()
      .IfReg0(aut->ImportVariable(*src->request_green()))
      .IfReg0(aut->ImportVariable(src->route_out()))
      .RunCallback(&src_alt_cb)
      .Rept(&Automata::Op::IfReg0, route_points)
      .RunCallback(&dst_cb)
      .ActReg1(aut->ImportVariable(src->request_green()));*/
}

MagnetCommandAutomata g_magnet_aut(&brd, *logic.allocator());
MagnetPause magnet_pause(&g_magnet_aut, &power_acc);

StandardFixedTurnout Turnout_In(&brd, EventBlock::Allocator(logic.allocator(),
                                                            "WIn", 40),
                                FixedTurnout::TURNOUT_CLOSED);
StandardFixedTurnout Turnout_Out(&brd, EventBlock::Allocator(logic.allocator(),
                                                             "WOut", 40),
                                 FixedTurnout::TURNOUT_CLOSED);

bool ign =
    BindPairs({
        {Rd1.block.side_b(), Rd2.block.side_a()},
        {Rd2.block.side_b(), Rd3.block.side_a()},
        {Rd3.block.side_b(), Turnout_In.b.side_points()},
        {Turnout_In.b.side_closed(), St2.block.side_a()},
        {Turnout_In.b.side_thrown(), St1.block.side_a()},
        {Turnout_Out.b.side_closed(), St2.block.side_b()},
        {Turnout_Out.b.side_thrown(), St1.block.side_b()},
        {Turnout_Out.b.side_points(), Rd1.block.side_a()},
            });


/*DefAut(XXleft, brd, {
  StateRef StWaiting(4);
  StateRef StTry1(NewUserState());
  StateRef StTry2(NewUserState());
  Def().IfState(StInit).ActState(StWaiting).ActTimer(4);
  Def().IfState(StWaiting).IfTimerDone().ActState(StTry1);

  StandardBlock* next = &Block_A301;

  Def()
      .IfReg0(ImportVariable(is_paused))
      .IfReg0(ImportVariable(next->route_in()))
      .IfReg0(ImportVariable(next->detector()))
      .IfState(StTry1)
      .IfReg0(ImportVariable(Block_XXB1.detector()))
      .IfReg1(ImportVariable(Block_XXB3.detector()))
      .ActState(StTry2);
  Def()
      .IfReg0(ImportVariable(is_paused))
      .IfReg0(ImportVariable(next->route_in()))
      .IfReg0(ImportVariable(next->detector()))
      .IfState(StTry2)
      .IfReg1(ImportVariable(Block_XXB1.detector()))
      .IfReg0(ImportVariable(Block_XXB3.detector()))
      .ActState(StTry1);

  Def()
      .IfReg0(ImportVariable(is_paused))
      .IfReg0(ImportVariable(next->route_in()))
      .IfReg0(ImportVariable(next->detector()))
      .IfState(StTry1)
      .IfReg1(ImportVariable(Block_XXB1.detector()))
      .ActReg1(ImportVariable(Block_XXB1.request_green()));

  Def()
      .IfReg0(ImportVariable(is_paused))
      .IfReg0(ImportVariable(next->route_in()))
      .IfReg0(ImportVariable(next->detector()))
      .IfState(StTry2)
      .IfReg1(ImportVariable(Block_XXB3.detector()))
      .ActReg1(ImportVariable(Block_XXB3.request_green()));

  // This flips the state 1<->2 when an outgoing route is set.
  Def().IfState(StTry1).IfReg1(ImportVariable(Block_XXB1.route_out())).ActState(
      StTry2);
  Def().IfState(StTry2).IfReg1(ImportVariable(Block_XXB3.route_out())).ActState(
      StTry1);
      });*/

/*DefAut(XXin, brd, {
  StateRef StWaiting(4);
  StateRef StTry1(NewUserState());
  StateRef StTry2(NewUserState());
  Def().IfState(StInit).ActState(StWaiting).ActTimer(4);
  Def().IfState(StWaiting).IfTimerDone().ActState(StTry1);

  StandardBlock* current = &Block_YYC23;
  StandardBlock* next;
  next = &Block_XXB1;
  Def()
      .IfReg0(ImportVariable(is_paused))
      .IfReg1(ImportVariable(current->detector()))
      .IfReg0(ImportVariable(Turnout_XXW8.state()))
      .IfReg0(ImportVariable(next->route_in()))
      .IfReg0(ImportVariable(next->detector()))
      .ActReg1(ImportVariable(current->request_green()));

  next = &Block_XXB3;
  Def()
      .IfReg0(ImportVariable(is_paused))
      .IfReg1(ImportVariable(current->detector()))
      .IfReg1(ImportVariable(Turnout_XXW8.state()))
      .IfReg0(ImportVariable(next->route_in()))
      .IfReg0(ImportVariable(next->detector()))
      .ActReg1(ImportVariable(current->request_green()));
      });*/

DefAut(display, brd, {
    /*  DefCopy(ImportVariable(Block_XXB1.detector()),
          ImportVariable(&panda_bridge.l0));
  DefCopy(ImportVariable(Block_A301.detector()),
          ImportVariable(&panda_bridge.l1));
  DefCopy(ImportVariable(Block_WWB14.detector()),
          ImportVariable(&panda_bridge.l2));
  DefCopy(ImportVariable(Block_YYC23.detector()),
  ImportVariable(&panda_bridge.l3));*/
});

void RgSignal(Automata* aut, const Automata::LocalVariable& route_set, Automata::LocalVariable* signal) {
  Def().IfReg0(route_set).ActSetValue(signal, 1, A_STOP);
  Def().IfReg1(route_set).ActSetValue(signal, 1, A_90);
}

void RedSignal(Automata* aut, Automata::LocalVariable* signal) {
  Def().ActSetValue(signal, 1, A_STOP);
}

void BlockSignal(Automata* aut, StandardBlock* block) {
  if (block->p()->main_sgn) {
    RgSignal(aut, aut->ImportVariable(block->route_out()),
             aut->ImportVariable(block->p()->main_sgn));
  }
  if (block->p()->adv_sgn) {
    RgSignal(aut, aut->ImportVariable(block->route_out()),
             aut->ImportVariable(block->p()->adv_sgn));
  }
  if (block->p()->in_adv_sgn) {
    RgSignal(aut, aut->ImportVariable(block->route_out()),
             aut->ImportVariable(block->p()->in_adv_sgn));
  }
  if (block->p()->r_main_sgn) {
    RedSignal(aut, aut->ImportVariable(block->p()->r_main_sgn));
  }
  if (block->p()->r_adv_sgn) {
    RedSignal(aut, aut->ImportVariable(block->p()->r_adv_sgn));
  }
  if (block->p()->r_in_adv_sgn) {
    RedSignal(aut, aut->ImportVariable(block->p()->r_in_adv_sgn));
  }
}

void TestPanel(Automata* aut, ConventionBlock* block, ConventionBlock* nextb) {
  auto* det = aut->ImportVariable(block->sensor_raw);
  auto* btn = aut->ImportVariable(block->button);
  Def().IfState(StInit).ActReg1(det).ActReg0(det).ActReg1(btn);
  
  auto* red = aut->ImportVariable(block->red);
  auto* green = aut->ImportVariable(block->green);
  Def().IfState(StInit).ActReg1(red).ActReg0(red);
  Def().IfState(StInit).ActReg1(green).ActReg0(green);
  
  auto& relay = aut->ImportVariable(*block->relay);
  auto* reqgrn = aut->ImportVariable(block->block.request_green());
  Def().IfReg0(aut->ImportVariable(*block->button)).ActReg1(reqgrn);
  Def().IfReg0(relay).IfReg0(*reqgrn).ActReg1(red);
  Def().IfReg0(relay).IfReg1(*reqgrn).ActReg0(red);
  Def().IfReg1(relay).ActReg0(red);
  aut->DefCopy(aut->ImportVariable(*block->relay),
               green);

  if (nextb) {
    auto& in_ctc = aut->ImportVariable(*ctc);
    Def()
        .IfReg0(in_ctc)
        .IfReg1(aut->ImportVariable(block->block.route_in()))
        .IfReg0(aut->ImportVariable(block->block.route_out()))
        .IfReg0(aut->ImportVariable(nextb->block.route_in()))
        .ActReg1(reqgrn);
  }
}

DefAut(signalaut, brd, {
    TestPanel(this, &Rd1, &Rd2);
    TestPanel(this, &Rd2, &Rd3);
    TestPanel(this, &Rd3, &St2);

    Def().IfState(StInit).ActState(StBase);
    /*    BlockSignal(this, &Block_XXB1);
    BlockSignal(this, &Block_XXB2);
    BlockSignal(this, &Block_XXB3);
    BlockSignal(this, &Block_YYB2);
    BlockSignal(this, &Block_YYC23);
    BlockSignal(this, &Block_WWB14);*/
  });

DefAut(signalaut1, brd, {
    TestPanel(this, &St1, nullptr);
    TestPanel(this, &St2, &Rd1);

    Def().IfState(StInit).ActState(StBase);
    /*    BlockSignal(this, &Block_A360);
    BlockSignal(this, &Block_A347);
    BlockSignal(this, &Block_A321);
    BlockSignal(this, &Block_A301)*/
  });

DefAut(signalaut2, brd, {
    /*BlockSignal(this, &Block_B421);
    BlockSignal(this, &Block_B447);
    BlockSignal(this, &Block_B475);*/
  });

int main(int argc, char** argv) {
  automata::reset_routes = &reset_all_routes;

  FILE* f = fopen("automata.bin", "wb");
  assert(f);
  string output;
  brd.Render(&output);
  fwrite(output.data(), 1, output.size(), f);
  fclose(f);

  //  f = fopen("convention-logic.cout", "wb");
  f = stdout;
  fprintf(f, "const char automata_code[] = {\n  ");
  int c = 0;
  for (char t : output) {
    fprintf(f, "0x%02x, ", (uint8_t)t);
    if (++c >= 12) {
      fprintf(f, "\n  ");
      c = 0;
    }
  }
  fprintf(f, "};\n");
  fclose(f);

  if (automata::GetOffsetMap()) {
    f = fopen("variables.txt", "w");
    assert(f);
    map<int, string>& m(*automata::GetOffsetMap());
    for (const auto& it : m) {
      fprintf(f, "%04x: %s\n", it.first * 2, it.second.c_str());
    }
    fclose(f);
  }

  f = fopen("jmri-out.xml", "w");
  assert(f);
  PrintAllEventVariables(f);
  fclose(f);

  f = fopen("var-bash-list.txt", "w");
  assert(f);
  PrintAllEventVariablesInBashFormat(f);
  fclose(f);



  return 0;
};
