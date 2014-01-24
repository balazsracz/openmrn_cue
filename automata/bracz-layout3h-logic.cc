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

I2CBoard b5(0x25), b6(0x26), b7(0x27), b1(0x21), b2(0x22), b3(0x23);
NativeIO n8(0x28);

/*StateRef StGreen(2);
StateRef StGoing(3);
*/

StateRef StUser1(10);
StateRef StUser2(11);

PandaControlBoard panda_bridge;

LPC11C lpc11_back;

I2CSignal signal_XXB2_main(&b3, 8, "XX.B2.main");
I2CSignal signal_XXB2_adv(&b3, 9, "XX.B2.adv");

I2CSignal signal_A301_main(&b5, 72, "A301.main");
I2CSignal signal_A301_adv(&b5, 73, "A301.adv");

I2CSignal signal_A321_main(&b2, 36, "A321.main");
I2CSignal signal_A321_adv(&b2, 37, "A321.adv");
I2CSignal signal_A421_main(&b2, 42, "A421.main");
I2CSignal signal_A421_adv(&b2, 43, "A421.adv");

I2CSignal signal_B321_main(&b2, 38, "B321.main");
I2CSignal signal_B321_adv(&b2, 39, "B321.adv");
I2CSignal signal_B421_main(&b2, 40, "B421.main");
I2CSignal signal_B421_adv(&b2, 41, "B421.adv");

I2CSignal signal_A347_main(&b2, 20, "A347.main");
I2CSignal signal_A347_adv(&b2, 21, "A347.adv");

I2CSignal signal_B447_main(&b2, 10, "B447.main");
I2CSignal signal_B447_adv(&b2, 11, "B447.adv");

I2CSignal signal_A360_main(&b2, 12, "A360.main");
I2CSignal signal_A360_adv(&b2, 13, "A360.adv");
I2CSignal signal_A460_main(&b2, 14, "A460.main");
I2CSignal signal_A460_adv(&b2, 15, "A460.adv");

I2CSignal signal_B360_adv(&b2, 17, "B360.adv");
I2CSignal signal_B460_adv(&b2, 19, "B460.adv");

I2CSignal signal_A375_adv(&b2, 75, "A375.adv");
I2CSignal signal_A475_adv(&b2, 74, "A475.adv");

I2CSignal signal_B375_main(&b2, 32, "B375.main");
I2CSignal signal_B375_adv(&b2, 33, "B375.adv");
I2CSignal signal_B475_main(&b2, 6, "B475.main");
I2CSignal signal_B475_adv(&b2, 7, "B475.adv");

I2CSignal signal_XXB1_main(&b1, 25, "XX.B1.main");
I2CSignal signal_XXB3_main(&b1, 24, "XX.B3.main");

I2CSignal signal_YYC23_main(&b3, 26, "YY.C23.main");
I2CSignal signal_YYC23_adv(&b3, 27, "YY.C23.adv");

I2CSignal signal_YYB2_main(&b7, 4, "YY.B2.main");
I2CSignal signal_YYB2_adv(&b7, 5, "YY.B2.adv");

I2CSignal signal_WWB14_main(&b5, 22, "WW.B14.main");
I2CSignal signal_WWB14_adv(&b5, 23, "WW.B14.adv");


PhysicalSignal A360(&b2.InBrownBrown, &b2.RelBlue,
                    &signal_A360_main.signal, &signal_A360_adv.signal,
                    &signal_B375_main.signal, &signal_B375_adv.signal,
                    &signal_A375_adv.signal, &signal_B360_adv.signal);
PhysicalSignal A347(&n8.d1, &n8.r1,
                    &signal_A347_main.signal, &signal_A347_adv.signal,
                    nullptr, nullptr, nullptr, nullptr);
PhysicalSignal A321(&n8.d3, &n8.r3,
                    &signal_A321_main.signal, &signal_A321_adv.signal,
                    nullptr, nullptr, nullptr, nullptr);
PhysicalSignal A301(&b6.InBrownGrey, &b6.RelGreen,
                    &signal_A301_main.signal, &signal_A301_adv.signal,
                    &signal_B321_main.signal, &signal_B321_adv.signal,
                    nullptr, nullptr);
PhysicalSignal WWB14(&b5.InBrownBrown, &b5.RelGreen,
                     &signal_WWB14_main.signal, &signal_WWB14_adv.signal,
                     nullptr, nullptr, nullptr, nullptr);
PhysicalSignal B421(&n8.d2, &n8.r2,
                    &signal_B421_main.signal, &signal_B421_adv.signal,
                    nullptr, nullptr, nullptr, nullptr);
PhysicalSignal B447(&n8.d0, &n8.r0,
                    &signal_B447_main.signal, &signal_B447_adv.signal,
                    &signal_A421_main.signal, &signal_A421_adv.signal,
                    nullptr, nullptr);
PhysicalSignal B475(&b2.InBrownGrey, &b2.RelGreen,
                    &signal_B475_main.signal, &signal_B475_adv.signal,
                    &signal_A460_main.signal, &signal_A460_adv.signal,
                    &signal_B460_adv.signal, &signal_A475_adv.signal);

PhysicalSignal YYC23(&b3.InBrownBrown, &b3.RelGreen,
                     &signal_YYC23_main.signal, &signal_YYC23_adv.signal,
                     nullptr, nullptr, nullptr, nullptr);

PhysicalSignal XXB1(&b1.InBrownGrey, &b1.RelGreen,
                    &signal_XXB1_main.signal, nullptr,
                    nullptr, nullptr, nullptr, nullptr);
PhysicalSignal XXB2(&b3.InBrownGrey, &b3.RelBlue,
                    &signal_XXB2_main.signal, &signal_XXB2_adv.signal,
                    nullptr, nullptr, nullptr, nullptr);
PhysicalSignal XXB3(&b1.InOraRed, &b1.RelBlue,
                    &signal_XXB3_main.signal, nullptr,
                    nullptr, nullptr, nullptr, nullptr);

PhysicalSignal YYB2(&b7.InBrownGrey, &b7.RelGreen,
                    &signal_YYB2_main.signal, &signal_YYB2_adv.signal,
                    nullptr, nullptr, nullptr, nullptr);

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

  DefCopy(*rep, ImportVariable(&b1.LedRed));
  DefCopy(*rep, ImportVariable(&b2.LedRed));
  DefCopy(*rep, ImportVariable(&b3.LedRed));
  DefCopy(*rep, ImportVariable(&b5.LedRed));
  DefCopy(*rep, ImportVariable(&b6.LedRed));
  DefCopy(*rep, ImportVariable(&b7.LedRed));
  DefCopy(*rep, ImportVariable(&panda_bridge.l4));
  DefCopy(*rep, ImportVariable(&lpc11_back.l0));
  DefCopy(*rep, ImportVariable(&n8.l0));
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

EventBlock logic(&brd, BRACZ_LAYOUT | 0xE000, "logic");

MagnetCommandAutomata g_magnet_aut(&brd, *logic.allocator());
MagnetPause magnet_pause(&g_magnet_aut, &power_acc);


StandardBlock Block_WWB14(&brd, &WWB14, EventBlock::Allocator(logic.allocator(),
                                                              "WW.B14", 80));
StandardBlock Block_A301(&brd, &A301,
                         EventBlock::Allocator(logic.allocator(), "A301", 80));
StandardBlock Block_A321(&brd, &A321,
                         EventBlock::Allocator(logic.allocator(), "A321", 80));
StandardBlock Block_A347(&brd, &A347,
                         EventBlock::Allocator(logic.allocator(), "A347", 80));
StandardBlock Block_A360(&brd, &A360,
                         EventBlock::Allocator(logic.allocator(), "A360", 80));
StandardBlock Block_B421(&brd, &B421,
                         EventBlock::Allocator(logic.allocator(), "B421", 80));
StandardBlock Block_B447(&brd, &B447,
                         EventBlock::Allocator(logic.allocator(), "B447", 80));
StandardBlock Block_B475(&brd, &B475,
                         EventBlock::Allocator(logic.allocator(), "B475", 80));

StandardMiddleDetector Det_380(&brd, &b7.InOraRed,
                               EventBlock::Allocator(logic.allocator(),
                                                     "Det380", 24, 8));

StandardFixedTurnout Turnout_W381(&brd, EventBlock::Allocator(logic.allocator(),
                                                              "W381", 40),
                                  FixedTurnout::TURNOUT_THROWN);
StandardFixedTurnout Turnout_W382(&brd, EventBlock::Allocator(logic.allocator(),
                                                              "W382", 40),
                                  FixedTurnout::TURNOUT_CLOSED);
MagnetDef Magnet_W481(&g_magnet_aut, "W481", &b7.ActBlueGrey, &b7.ActBlueBrown);
StandardMovableTurnout Turnout_W481(
    &brd, EventBlock::Allocator(logic.allocator(), "W481", 40), &Magnet_W481);

StandardBlock Block_YYB2(&brd, &YYB2,
                         EventBlock::Allocator(logic.allocator(), "YY.B2", 80));
StandardBlock Block_YYC23(&brd, &YYC23, EventBlock::Allocator(logic.allocator(),
                                                              "YY.C23", 80));
StandardFixedTurnout Turnout_YYW6(&brd, EventBlock::Allocator(logic.allocator(),
                                                              "YY.W6", 40),
                                  FixedTurnout::TURNOUT_THROWN);

StandardFixedTurnout Turnout_XXW1(&brd, EventBlock::Allocator(logic.allocator(),
                                                              "XX.W1", 40),
                                  FixedTurnout::TURNOUT_THROWN);
StandardFixedTurnout Turnout_XXW2(&brd, EventBlock::Allocator(logic.allocator(),
                                                              "XX.W2", 40),
                                  FixedTurnout::TURNOUT_THROWN);
MagnetDef Magnet_XXW7(&g_magnet_aut, "XX.W7", &b3.ActBlueGrey,
                      &b3.ActBlueBrown);
StandardFixedTurnout Turnout_XXW7(
    &brd, EventBlock::Allocator(logic.allocator(), "XX.W7", 40),
    FixedTurnout::TURNOUT_CLOSED);  // we ignore the magnets here
MagnetDef Magnet_XXW8(&g_magnet_aut, "XX.W8", &b3.ActOraGreen, &b3.ActOraRed);
StandardMovableTurnout Turnout_XXW8(
    &brd, EventBlock::Allocator(logic.allocator(), "XX.W8", 40), &Magnet_XXW8);

StandardBlock Block_XXB1(&brd, &XXB1,
                         EventBlock::Allocator(logic.allocator(), "XX.B1", 80));
StandardBlock Block_XXB2(&brd, &XXB2,
                         EventBlock::Allocator(logic.allocator(), "XX.B2", 80));
StandardBlock Block_XXB3(&brd, &XXB3,
                         EventBlock::Allocator(logic.allocator(), "XX.B3", 80));

#define BLOCK_SEQUENCE &Block_A360, &Block_A347, &Block_A321, &Block_A301, &Block_WWB14, &Block_B421, &Block_B447, &Block_B475

std::vector<StandardBlock*> block_sequence = {BLOCK_SEQUENCE};

bool ignored1 = BindSequence({BLOCK_SEQUENCE});
/*bool ignored2 = Block_YYC23.side_b()->Bind(Turnout_XXW8.b.side_points());
bool ignored3 = Block_XXB1.side_a()->Bind(Turnout_XXW8.b.side_closed());
bool ignored4 = Block_XXB3.side_a()->Bind(Turnout_XXW8.b.side_thrown());
bool ignored5 = Block_A301.side_a()->Bind(Turnout_XXW1.b.side_points());
bool ignored6 = Block_XXB1.side_b()->Bind(Turnout_XXW1.b.side_closed());
bool ignored7 = Block_XXB3.side_b()->Bind(Turnout_XXW1.b.side_thrown());*/

bool ign =
    BindPairs({{Block_YYC23.side_b(), Turnout_YYW6.b.side_closed()},
               {Block_YYB2.side_a(), Turnout_YYW6.b.side_thrown()},
               {Turnout_YYW6.b.side_points(), Turnout_XXW8.b.side_points()},
               {Block_XXB1.side_a(), Turnout_XXW8.b.side_closed()},
               {Turnout_XXW7.b.side_points(), Turnout_XXW8.b.side_thrown()},
               {Block_XXB2.side_b(), Turnout_XXW7.b.side_thrown()},
               {Block_XXB3.side_a(), Turnout_XXW7.b.side_closed()},
               {Block_XXB1.side_b(), Turnout_XXW1.b.side_closed()},
               {Block_XXB2.side_a(), Turnout_XXW2.b.side_thrown()},
               {Block_XXB3.side_b(), Turnout_XXW2.b.side_closed()},
               {Turnout_XXW1.b.side_thrown(), Turnout_XXW2.b.side_points()},
               {Turnout_XXW1.b.side_points(), Turnout_W382.b.side_closed()},
               {Turnout_W382.b.side_thrown(), Block_YYB2.side_b()},
               {Turnout_W382.b.side_points(), Turnout_W381.b.side_points()},
               {Turnout_W381.b.side_thrown(), Det_380.side_a()},
               {Det_380.side_b(), Block_A360.side_a()},
               {Turnout_W381.b.side_closed(), Turnout_W481.b.side_thrown()},
               {Block_YYC23.side_a(), Turnout_W481.b.side_closed()},
               {Block_B475.side_b(), Turnout_W481.b.side_points()}, });

DefAut(control_logic, brd, {
  StateRef StWaiting(4);
  Def().IfState(StInit).ActState(StWaiting).ActTimer(4).ActReg1(
      ImportVariable(&is_paused));
  for (size_t i = 0; i < block_sequence.size() - 1; ++i) {
    SimpleFollowStrategy(block_sequence[i], block_sequence[i + 1], {}, this);
  }
  /*SimpleFollowStrategy(&Block_XXB2, &Block_YYB2, {Turnout_XXW8.b.any_route()},
                       this);
  SimpleFollowStrategy(&Block_YYB2, &Block_A301, {Turnout_W382.b.any_route()},
  this);*/
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
  DefCopy(ImportVariable(Block_XXB1.detector()),
          ImportVariable(&panda_bridge.l0));
  DefCopy(ImportVariable(Block_A301.detector()),
          ImportVariable(&panda_bridge.l1));
  DefCopy(ImportVariable(Block_WWB14.detector()),
          ImportVariable(&panda_bridge.l2));
  DefCopy(ImportVariable(Block_YYC23.detector()),
          ImportVariable(&panda_bridge.l3));
});

/*
DefAut(returnloop, brd, {
  static GlobalVariable* request_pending = NewTempVariable(&brd);
  LocalVariable* pending = ImportVariable(request_pending);

  static StateRef StFront1(NewUserState());  // YYC23 -> XXB1
  static StateRef StFront3(NewUserState());  // YYC23 -> XXB3
  static StateRef StBack1(NewUserState());   // XXB2 -> YYB2
  static StateRef StBack2(NewUserState());   // alias for the above to guarantee
                                      // fairness

  static StateRef SettingFront1(NewUserState());
  static StateRef SettingFront3(NewUserState());
  static StateRef SettingBack1(NewUserState());
  static StateRef SettingBack2(NewUserState());

  Def().IfState(StInit).ActReg0(pending).ActState(StFront1);

  Def()
      .IfReg0(busy)
      .IfState(StFront1)
      .RunCallback(&yyc23_depart)
      .RunCallback(&xxb1_arrive)
      .ActState(SettingFront1);
  Def()
      .IfReg0(busy)
      .IfState(StFront1)
      // We failed to set stfront1, source train is missing or dst is busy.
      // Let's try to send a train to the other front track.
      .RunCallback(&yyc23_depart)
      .RunCallback(&xxb3_arrive)
      // Now we can run a train to the other track. Let's do that.
      .ActState(SettingFront3);

  Def()
      .IfReg0(busy)
      .IfState(StFront1)
      // We failed to run a train to the front. Let's try to run one to the
      // back.
      .RunCallback(&xxb2_depart)
      .RunCallback(&yyb2_arrive)
      .ActState(StBack2);

  Def()
      .IfReg0(busy)
      .IfState(StFront3)
      .RunCallback(&yyc23_depart)
      .RunCallback(&xxb3_arrive)
      .ActState(SettingFront3);
  Def()
      .IfReg0(busy)
      .IfState(StFront3)
      // We failed to set stfront3, source train is missing or dst is busy.
      // Let's try to send a train to the other front track.
      .RunCallback(&yyc23_depart)
      .RunCallback(&xxb1_arrive)
      // Now we can run a train to the other track. Let's do that.
      .ActState(SettingFront1);

  Def()
      .IfReg0(busy)
      .IfState(StFront3)
      // We failed to run a train to the front. Let's try to run one to the
      // back.
      .RunCallback(&xxb2_depart)
      .RunCallback(&yyb2_arrive)
      .ActState(StBack1);

  Def()
      .IfReg0(busy)
      .IfReg0(*pending)
      .IfState(StBack1)
      .RunCallback(&xxb2_depart)
      .RunCallback(&yyb2_arrive)
      .ActReg1(xxb2_reqgreen)
      .ActReg1(pending);

  Def()
      .IfState(StBack1)
      .IfReg1(*pending)
      .IfReg0(*xxb2_reqgreen)
      .ActReg0(pending)
      .ActState(StFront3);

  Def()
      .IfReg0(busy)
      .IfReg0(*pending)
      .IfState(StBack2)
      .RunCallback(&xxb2_depart)
      .RunCallback(&yyb2_arrive)
      .ActReg1(xxb2_reqgreen)
      .ActReg1(pending);

  Def()
      .IfState(StBack2)
      .IfReg1(*pending)
      .IfReg0(*xxb2_reqgreen)
      .ActReg0(pending)
      .ActState(StFront1);

  Def().IfReg0(busy).IfReg0(*pending).IfState(SettingFront1).ActReg0(xxw8_cmd);
  Def()
      .IfReg0(busy)
      .IfReg0(*pending)
      .IfState(SettingFront1)
      .IfReg0(xxw8_state)
      .ActReg1(yyc23_reqgreen)
      .ActReg1(pending);
  Def().IfState(SettingFront1)
      .IfReg1(*pending)
      .IfReg0(*yyc23_reqgreen)
      .ActReg0(pending)
      .ActState(StBack1);

  Def().IfReg0(busy).IfReg0(*pending).IfState(SettingFront3).ActReg1(xxw8_cmd);
  Def()
      .IfReg0(busy)
      .IfReg0(*pending)
      .IfState(SettingFront3)
      .IfReg1(xxw8_state)
      .ActReg1(yyc23_reqgreen)
      .ActReg1(pending);
  Def().IfState(SettingFront3)
      .IfReg1(*pending)
      .IfReg0(*yyc23_reqgreen)
      .ActReg0(pending)
      .ActState(StBack2);


      });*/

DefAut(returnloop1, brd, {
  static EventBlock::Allocator a(logic.allocator(), "interlocking.loop", 4);
  // 1 if the last train went from back->front.
  static GlobalVariable* v_last_to_front = a.Allocate("last_to_front");
  LocalVariable* last_to_front = ImportVariable(v_last_to_front);
  // 1 if the last back->front train went to track 1.
  static GlobalVariable* v_last_front_t1 = a.Allocate("last_to_front_t1");
  LocalVariable* last_front_t1 = ImportVariable(v_last_front_t1);

  auto xxb2_depart = NewCallback(&IfSourceTrackReady, &Block_XXB2);
  auto yyc23_depart = NewCallback(&IfSourceTrackReady, &Block_YYC23);

  auto yyb2_arrive = NewCallback(&IfDstTrackReady, &Block_YYB2);
  auto xxb1_arrive = NewCallback(&IfDstTrackReady, &Block_XXB1);
  auto xxb3_arrive = NewCallback(&IfDstTrackReady, &Block_XXB3);

  const LocalVariable& busy = ImportVariable(*Turnout_XXW8.b.any_route());

  LocalVariable* xxw8_cmd = ImportVariable(Magnet_XXW8.command.get());
  const LocalVariable& xxw8_state = ImportVariable(*Magnet_XXW8.current_state);

  LocalVariable* yyc23_reqgreen = ImportVariable(Block_YYC23.request_green());
  LocalVariable* xxb2_reqgreen = ImportVariable(Block_XXB2.request_green());

  Def().IfState(StInit).ActState(StBase);

  static StateRef StTrySendTrain(NewUserState());
  static StateRef StTryFrontToBack(NewUserState());
  static StateRef StTryBackToFront(NewUserState());

  static StateRef StFrontPending(NewUserState());
  static StateRef StBackPending(NewUserState());

  static StateRef StSendToXXB1(NewUserState());
  static StateRef StSendToXXB3(NewUserState());

  // If the circle is free, let's look for a train to send.
  Def().IfState(StBase).IfReg0(busy).ActState(StTrySendTrain);

  // We check which way the last train went, and try to go the other way now.
  Def().IfState(StTrySendTrain).IfReg1(*last_to_front).ActState(
      StTryFrontToBack);

  Def().IfState(StTrySendTrain).ActState(StTryBackToFront);

  // Checks if we can send a train front->back.
  Def()
      .IfState(StTryFrontToBack)
      .RunCallback(&xxb2_depart)
      .RunCallback(&yyb2_arrive)
      .ActReg1(xxb2_reqgreen)
      .ActReg0(last_to_front)
      .ActState(StFrontPending);

  Def()
      .IfState(StTryFrontToBack)
      // We failed to send front to back. Try reverse.
      .IfReg0(*last_to_front)
      .ActState(StTryBackToFront);

  Def()
      .IfState(StTryFrontToBack)
      // We failed to send any train. Try again later.
      .ActState(StBase);

  // When the route setting is completed, return to the starting state.
  Def()
      .IfState(StFrontPending)
      .IfReg0(*xxb2_reqgreen)
      .ActReg0(last_to_front)
      .ActState(StBase);

  // Trying to send a train back to front. Check if our preferred direction is
  // available.
  Def()
      .IfState(StTryBackToFront)
      .IfReg0(*last_front_t1)
      .RunCallback(&yyc23_depart)
      .RunCallback(&xxb1_arrive)
      .ActState(StSendToXXB1);

  Def()
      .IfState(StTryBackToFront)
      .IfReg1(*last_front_t1)
      .RunCallback(&yyc23_depart)
      .RunCallback(&xxb3_arrive)
      .ActState(StSendToXXB3);

  // The same conditions as above, but without the restriction of the last
  // direction.
  Def()
      .IfState(StTryBackToFront)
      .RunCallback(&yyc23_depart)
      .RunCallback(&xxb1_arrive)
      .ActState(StSendToXXB1);

  Def()
      .IfState(StTryBackToFront)
      .RunCallback(&yyc23_depart)
      .RunCallback(&xxb3_arrive)
      .ActState(StSendToXXB3);

  Def()
      .IfState(StTryBackToFront)
      // back->front failed, look other way
      .IfReg1(*last_to_front)  // if we haven't yet done so
      .ActState(StTryFrontToBack);

  Def()
      .IfState(StTryBackToFront)
      // Try again later.
      .ActState(StBase);

  Def().IfState(StSendToXXB1).ActReg0(xxw8_cmd);

  Def()
      .IfState(StSendToXXB1)
      .IfReg0(xxw8_state)
      .ActReg1(yyc23_reqgreen)
      .ActReg1(last_front_t1)
      .ActState(StBackPending);

  Def().IfState(StSendToXXB3).ActReg1(xxw8_cmd);

  Def()
      .IfState(StSendToXXB3)
      .IfReg1(xxw8_state)
      .ActReg1(yyc23_reqgreen)
      .ActReg0(last_front_t1)
      .ActState(StBackPending);

  Def()
      .IfState(StBackPending)
      .IfReg0(*yyc23_reqgreen)
      .ActReg1(last_to_front)
      .ActState(StBase);
});

DefAut(front_exclusion, brd, {
  static EventBlock::Allocator a(logic.allocator(), "interlocking.front", 4);
  // 1 if the last inbound train went to XX, 0 if to YY.
  static GlobalVariable* v_last_to_front = a.Allocate("last_in_to_front");
  LocalVariable* last_to_front = ImportVariable(v_last_to_front);
  // 1 if the last outbound train went from XX, 0 if from YY.
  static GlobalVariable* v_last_from_front = a.Allocate("last_out_from_front");
  LocalVariable* last_from_front = ImportVariable(v_last_from_front);
  // 1 if the last outbound front train went from track 1.
  static GlobalVariable* v_last_front_t1 = a.Allocate("last_out_from_front_t1");
  LocalVariable* last_front_t1 = ImportVariable(v_last_front_t1);

  // External interface. These trigger our main loops.
  auto b475_depart = NewCallback(&IfSourceTrackReady, &Block_B475);
  auto a360_arrive = NewCallback(&IfDstTrackReady, &Block_A360);

  // Inbound conditions.
  auto yyc23_arrive = NewCallback(&IfDstTrackReady, &Block_YYC23);
  auto xxb2_arrive = NewCallback(&IfDstTrackReady, &Block_XXB2);

  // Outbound conditions.
  auto xxb1_depart = NewCallback(&IfSourceTrackReady, &Block_XXB1);
  auto xxb3_depart = NewCallback(&IfSourceTrackReady, &Block_XXB3);
  auto yyb2_depart = NewCallback(&IfSourceTrackReady, &Block_YYB2);

  // These will tell us if the back / front track is available.
  const LocalVariable& busy_4 = ImportVariable(*Turnout_W481.b.any_route());
  const LocalVariable& busy_3 = ImportVariable(*Turnout_W382.b.any_route());

  LocalVariable* w481_cmd = ImportVariable(Magnet_W481.command.get());
  const LocalVariable& w481_state = ImportVariable(*Magnet_W481.current_state);

  // Outbound go.
  LocalVariable* xxb1_reqgreen = ImportVariable(Block_XXB1.request_green());
  LocalVariable* xxb3_reqgreen = ImportVariable(Block_XXB3.request_green());
  LocalVariable* yyb2_reqgreen = ImportVariable(Block_YYB2.request_green());
  // Inbound go.
  LocalVariable* b475_reqgreen = ImportVariable(Block_B475.request_green());

  static StateRef StTryInbound(NewUserState());
  static StateRef StWaitInboundToXXB2(NewUserState());
  static StateRef StRunInboundToXXB2(NewUserState());
  static StateRef StPendingInbound(NewUserState());
  static StateRef StInboundToYYC23(NewUserState());
  static StateRef StRunInboundToYYC23(NewUserState());
  static StateRef StTryOutbound(NewUserState());
  static StateRef StPendingYYB2(NewUserState());
  static StateRef StPendingXXB1(NewUserState());
  static StateRef StPendingXXB3(NewUserState());
  /*static StateRef (NewUserState());
  static StateRef (NewUserState());
  static StateRef (NewUserState());
  static StateRef (NewUserState());*/


  Def().IfState(StInit).ActState(StBase);

  Def().IfState(StBase).RunCallback(&b475_depart).IfReg0(busy_4).ActState(
      StTryInbound);

  Def().IfState(StBase).RunCallback(&a360_arrive).IfReg0(busy_3).ActState(
      StTryOutbound);

  // Pick inbound trains (front or back). First try with the preference.
  Def().IfState(StTryInbound).IfReg0(*last_to_front).RunCallback(&xxb2_arrive)
      .ActState(StWaitInboundToXXB2);

  Def().IfState(StTryInbound).IfReg1(*last_to_front).RunCallback(&yyc23_arrive)
      .IfReg0(busy_4)
      .ActState(StInboundToYYC23);

  // Then try without the preference.
  Def().IfState(StTryInbound).RunCallback(&xxb2_arrive)
      .IfReg0(busy_4)  // we essentially skip the wait here.
      .IfReg0(busy_3)
      .ActState(StWaitInboundToXXB2);

  Def().IfState(StTryInbound).RunCallback(&yyc23_arrive)
      .IfReg0(busy_4)
      .ActState(StInboundToYYC23);

  // No space or no track for inbound trains. Try outbound.
  Def().IfState(StTryInbound)
      .IfReg0(busy_3)
      .RunCallback(&a360_arrive)
      .ActState(StTryOutbound);
  // fail.
  Def().IfState(StTryInbound)
      .ActState(StBase);


  // This wait state purposefully doesn't have an exit. We know that the
  // destination track is free and the source track has a train. We just have
  // to wait for the interlocking to be free and we are willing to wait that
  // out.
  Def().IfState(StWaitInboundToXXB2)
      .IfReg0(busy_4)
      .IfReg0(busy_3)
      .RunCallback(&b475_depart)
      .RunCallback(&xxb2_arrive)
      .ActReg1(w481_cmd)
      .ActState(StRunInboundToXXB2);

  // Waits for the turnout to reach the end position.
  Def().IfState(StRunInboundToXXB2)
      .IfReg1(w481_state)
      .ActReg1(b475_reqgreen)
      .ActReg1(last_to_front)
      .ActState(StPendingInbound);

  // Waits for the route setting to complete.
  Def().IfState(StPendingInbound)
      .IfReg0(*b475_reqgreen)
      .ActState(StBase);

  Def().IfState(StInboundToYYC23)
      .RunCallback(&b475_depart)
      .RunCallback(&yyc23_arrive)
      .ActReg0(w481_cmd)
      .ActState(StRunInboundToYYC23);

  Def().IfState(StRunInboundToYYC23)
      .IfReg0(w481_state)
      .ActReg1(b475_reqgreen)
      .ActReg0(last_to_front)
      .ActState(StPendingInbound);

  //  ====== outbound trains ======

  Def().IfState(StTryOutbound)
      .IfReg1(busy_3)  // no track.
      .ActState(StBase);

  // back
  Def().IfState(StTryOutbound)
      .IfReg1(*last_from_front)
      .RunCallback(&yyb2_depart)
      .ActReg1(yyb2_reqgreen)
      .ActState(StPendingYYB2);

  // front  x2
  Def().IfState(StTryOutbound)
      .IfReg0(*last_from_front)
      .IfReg1(*last_front_t1)
      .RunCallback(&xxb3_depart)
      .ActReg1(xxb3_reqgreen)
      .ActState(StPendingXXB3);

  Def().IfState(StTryOutbound)
      .IfReg0(*last_from_front)
      .IfReg0(*last_front_t1)
      .RunCallback(&xxb1_depart)
      .ActReg1(xxb1_reqgreen)
      .ActState(StPendingXXB1);
  // front without track preference
  Def().IfState(StTryOutbound)
      .IfReg0(*last_from_front)
      .RunCallback(&xxb3_depart)
      .ActReg1(xxb3_reqgreen)
      .ActState(StPendingXXB3);

  Def().IfState(StTryOutbound)
      .IfReg0(*last_from_front)
      .RunCallback(&xxb1_depart)
      .ActReg1(xxb1_reqgreen)
      .ActState(StPendingXXB1);

  // back witnout any preference
  Def().IfState(StTryOutbound)
      .RunCallback(&yyb2_depart)
      .ActReg1(yyb2_reqgreen)
      .ActState(StPendingYYB2);

  // front without any preference
  Def().IfState(StTryOutbound)
      .RunCallback(&xxb3_depart)
      .ActReg1(xxb3_reqgreen)
      .ActState(StPendingXXB3);

  Def().IfState(StTryOutbound)
      .RunCallback(&xxb1_depart)
      .ActReg1(xxb1_reqgreen)
      .ActState(StPendingXXB1);

  // No trains found anywhere, go back to base state and try an inbound.
  Def().IfState(StTryOutbound)
      .ActState(StBase);

  // Outbound pending closedown.
  Def().IfState(StPendingYYB2)
      .IfReg0(*yyb2_reqgreen)
      .ActReg0(last_from_front)
      .ActState(StBase);

  Def().IfState(StPendingXXB1)
      .IfReg0(*xxb1_reqgreen)
      .ActReg1(last_from_front)
      .ActReg1(last_front_t1)
      .ActState(StBase);

  Def().IfState(StPendingXXB3)
      .IfReg0(*xxb3_reqgreen)
      .ActReg1(last_from_front)
      .ActReg0(last_front_t1)
      .ActState(StBase);

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

DefAut(signalaut, brd, {
    BlockSignal(this, &Block_XXB1);
    BlockSignal(this, &Block_XXB2);
    BlockSignal(this, &Block_XXB3);
    BlockSignal(this, &Block_YYB2);
    BlockSignal(this, &Block_YYC23);
    BlockSignal(this, &Block_WWB14);
  });

DefAut(signalaut2, brd, {
    BlockSignal(this, &Block_A360);
    BlockSignal(this, &Block_A347);
    BlockSignal(this, &Block_A321);
    BlockSignal(this, &Block_A301);
    BlockSignal(this, &Block_B421);
    BlockSignal(this, &Block_B447);
    BlockSignal(this, &Block_B475);
  });
    /*    RgSignal(this, ImportVariable(Block_XXB2.route_out()),
             ImportVariable(&signal_XXB2_main.signal));
    RgSignal(this, ImportVariable(Block_XXB2.route_out()),
             ImportVariable(&signal_XXB2_adv.signal));

    RgSignal(this, ImportVariable(Block_A360.route_out()),
             ImportVariable(&signal_A360_main.signal));
    RgSignal(this, ImportVariable(Block_A360.route_out()),
             ImportVariable(&signal_A375_adv.signal));

    RgSignal(this, ImportVariable(Block_A347.route_in()),
             ImportVariable(&signal_A360_adv.signal));

    RgSignal(this, ImportVariable(Block_A347.route_out()),
             ImportVariable(&signal_A347_main.signal));
    RgSignal(this, ImportVariable(Block_A321.route_in()),
             ImportVariable(&signal_A347_adv.signal));

    RgSignal(this, ImportVariable(Block_A321.route_out()),
             ImportVariable(&signal_A321_main.signal));
    RgSignal(this, ImportVariable(Block_A301.route_in()),
             ImportVariable(&signal_A321_adv.signal));

    RgSignal(this, ImportVariable(Block_B475.route_out()),
             ImportVariable(&signal_B475_main.signal));
    RgSignal(this, ImportVariable(*Turnout_W481.b.any_route()),
             ImportVariable(&signal_B475_adv.signal));

    RgSignal(this, ImportVariable(Block_XXB1.route_out()),
             ImportVariable(&signal_XXB1_main.signal));
    RgSignal(this, ImportVariable(Block_XXB3.route_out()),
             ImportVariable(&signal_XXB3_main.signal));

    RedSignal(this, ImportVariable(&signal_B375_main.signal));
    RedSignal(this, ImportVariable(&signal_B375_adv.signal));*/

int main(int argc, char** argv) {
  automata::reset_routes = &reset_all_routes;

  FILE* f = fopen("automata.bin", "wb");
  assert(f);
  string output;
  brd.Render(&output);
  fwrite(output.data(), 1, output.size(), f);
  fclose(f);

  f = fopen("automata.cout", "wb");
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

  f = fopen("variables.txt", "w");
  assert(f);
  map<int, string>& m(*automata::GetOffsetMap());
  for (const auto& it : m) {
    fprintf(f, "%04x: %s\n", it.first * 2, it.second.c_str());
  }
  fclose(f);

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
