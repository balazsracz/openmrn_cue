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

static const uint64_t NODE_ID_DCC = 0x060100000000ULL;

//#define OFS_GLOBAL_BITS 24

#ifndef OVERRIDE
#define OVERRIDE
#endif

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

I2CBoard b5(0x25), b6(0x26), b7(0x27), b1(0x21), b2(0x22);
NativeIO n8(0x28);
AccBoard ba(0x2a), bb(0x2b);

/*StateRef StGreen(2);
StateRef StGoing(3);
*/

StateRef StUser1(10);
StateRef StUser2(11);

PandaControlBoard panda_bridge;

LPC11C lpc11_back;

I2CSignal signal_XXB2_main(&b1, 8, "XX.B2.main");  // was: b3
I2CSignal signal_XXB2_adv(&b1, 9, "XX.B2.adv");  // was: b3

I2CSignal signal_A301_main(&b5, 72, "A301.main");
I2CSignal signal_A301_adv(&b5, 73, "A301.adv");

I2CSignal signal_A321_main(&bb, 36, "A321.main");
I2CSignal signal_A321_adv(&bb, 37, "A321.adv");
I2CSignal signal_A421_main(&bb, 42, "A421.main");
I2CSignal signal_A421_adv(&bb, 43, "A421.adv");

I2CSignal signal_B321_main(&bb, 38, "B321.main");
I2CSignal signal_B321_adv(&bb, 39, "B321.adv");
I2CSignal signal_B421_main(&bb, 40, "B421.main");
I2CSignal signal_B421_adv(&bb, 41, "B421.adv");

I2CSignal signal_A347_main(&bb, 20, "A347.main");
I2CSignal signal_A347_adv(&bb, 21, "A347.adv");

I2CSignal signal_B447_main(&bb, 10, "B447.main");
I2CSignal signal_B447_adv(&bb, 11, "B447.adv");

I2CSignal signal_A360_main(&bb, /*12*/142, "A360.main");
I2CSignal signal_A360_adv(&bb, 13, "A360.adv");
I2CSignal signal_A460_main(&bb, /*14*/139, "A460.main");
I2CSignal signal_A460_adv(&bb, 15, "A460.adv");

I2CSignal signal_B360_adv(&bb, 17, "B360.adv");
I2CSignal signal_B460_adv(&bb, 19, "B460.adv");

I2CSignal signal_A375_adv(&bb, 75, "A375.adv");
I2CSignal signal_A475_adv(&bb, 74, "A475.adv");

I2CSignal signal_B375_main(&bb, 32, "B375.main");
I2CSignal signal_B375_adv(&bb, 33, "B375.adv");
I2CSignal signal_B475_main(&bb, 6, "B475.main");
I2CSignal signal_B475_adv(&bb, 7, "B475.adv");

I2CSignal signal_XXB1_main(&b1, 25, "XX.B1.main");
I2CSignal signal_XXB3_main(&b1, 24, "XX.B3.main");

I2CSignal signal_YYC23_main(&b1, 26, "YY.C23.main"); // was: b3
I2CSignal signal_YYC23_adv(&b1, 27, "YY.C23.adv");  // was: b3

I2CSignal signal_YYB2_main(&b7, 4, "YY.B2.main");
I2CSignal signal_YYB2_adv(&b7, 5, "YY.B2.adv");

I2CSignal signal_WWB14_main(&b5, 22, "WW.B14.main");
I2CSignal signal_WWB14_adv(&b5, 23, "WW.B14.adv");

//I2CSignal signal_WWB3_main(&b5, 75, "WW.B3.main");  // todo:
//I2CSignal signal_WWB3_adv(&b5, 74, "WW.B3.adv");


PhysicalSignal A360(&bb.InBrownBrown, &bb.Rel0,
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
PhysicalSignal WWB3(&b6.InOraGreen, &b6.RelBlue,
                    nullptr, nullptr,
                    nullptr, nullptr, nullptr, nullptr);
PhysicalSignal WWB2(&b6.InOraRed, &b5.RelBlue,
                    nullptr, nullptr,
                    nullptr, nullptr, nullptr, nullptr);
PhysicalSignal WWA11(&b6.InBrownBrown, &b6.LedGreen,
                     &signal_WWB14_main.signal, &signal_WWB14_adv.signal,
                     nullptr, nullptr, nullptr, nullptr);

PhysicalSignal B421(&n8.d2, &n8.r2,
                    &signal_B421_main.signal, &signal_B421_adv.signal,
                    nullptr, nullptr, nullptr, nullptr);
PhysicalSignal B447(&n8.d0, &n8.r0,
                    &signal_B447_main.signal, &signal_B447_adv.signal,
                    &signal_A421_main.signal, &signal_A421_adv.signal,
                    nullptr, nullptr);
PhysicalSignal B460(&bb.InGreenGreen, &bb.Rel2, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr);
PhysicalSignal B475(&bb.InBrownGrey, &bb.Rel1,
                    &signal_B475_main.signal, &signal_B475_adv.signal,
                    &signal_A460_main.signal, &signal_A460_adv.signal,
                    &signal_B460_adv.signal, &signal_A475_adv.signal);

PhysicalSignal ZZA2(&bb.InOraGreen, &bb.LedGreen,
                    nullptr, nullptr,
                    nullptr, nullptr, nullptr, nullptr);

PhysicalSignal ZZA3(&bb.InOraRed, &bb.LedYellow,
                    nullptr, nullptr,
                    nullptr, nullptr, nullptr, nullptr);

PhysicalSignal YYA3(&b1.InBrownBrown, &b1.LedGreen,
                    nullptr, nullptr,
                    nullptr, nullptr, nullptr, nullptr);

PhysicalSignal YYC23(&ba.In0, &ba.Rel3,
                     &signal_YYC23_main.signal, &signal_YYC23_adv.signal,
                     nullptr, nullptr, nullptr, nullptr);

PhysicalSignal XXB1(&b1.InBrownGrey, &b1.RelGreen,
                    &signal_XXB1_main.signal, nullptr,
                    nullptr, nullptr, nullptr, nullptr);
PhysicalSignal XXB2(&ba.In3, &ba.Rel2,
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
    Def().IfState(StInit).ActReg1(ImportVariable(&is_paused)).ActState(StBase);
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
  DefCopy(*rep, ImportVariable(&bb.LedGoldSw));
  DefCopy(*rep, ImportVariable(&ba.LedGoldSw));
  DefCopy(*rep, ImportVariable(&b5.LedRed));
  DefCopy(*rep, ImportVariable(&b6.LedRed));
  DefCopy(*rep, ImportVariable(&b7.LedRed));
  DefCopy(*rep, ImportVariable(&panda_bridge.l4));
  DefCopy(*rep, ImportVariable(&lpc11_back.l0));
  DefCopy(*rep, ImportVariable(&n8.l0));
  DefCopy(ImportVariable(b5.InBrownGrey), ImportVariable(&b5.LedGreen));
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

void IfNotPaused(Automata::Op* op) {
  op->IfReg0(op->parent()->ImportVariable(is_paused))
      .IfReg0(op->parent()->ImportVariable(reset_all_routes));
}

auto g_not_paused_condition = NewCallback(&IfNotPaused);

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

EventBlock perm(&brd, BRACZ_LAYOUT | 0xC000, "perm", 1024 / 2);
EventBlock logic2(&brd, BRACZ_LAYOUT | 0xD000, "logic2");
EventBlock logic(&brd, BRACZ_LAYOUT | 0xE000, "logic");
EventBlock::Allocator& train_perm(*perm.allocator());
EventBlock::Allocator train_tmp(logic2.allocator()->Allocate("train", 384));

MagnetCommandAutomata g_magnet_aut(&brd, *logic2.allocator());
MagnetPause magnet_pause(&g_magnet_aut, &power_acc);


MagnetDef Magnet_WWW1(&g_magnet_aut, "WW.W1", &b6.ActGreenGreen, &b6.ActGreenRed);
StandardMovableTurnout Turnout_WWW1(
    &brd, EventBlock::Allocator(logic.allocator(), "WW.W1", 40), &Magnet_WWW1);

StandardFixedTurnout Turnout_WWW2(&brd, EventBlock::Allocator(logic.allocator(),
                                                              "WW.W2", 40),
                                  FixedTurnout::TURNOUT_CLOSED);
StandardFixedTurnout Turnout_WWW3(&brd, EventBlock::Allocator(logic.allocator(),
                                                              "WW.W3", 40),
                                  FixedTurnout::TURNOUT_CLOSED);
MagnetDef Magnet_WWW4(&g_magnet_aut, "WW.W4", &b5.ActOraGreen, &b5.ActOraRed);
StandardMovableDKW DKW_WWW4(&brd, EventBlock::Allocator(logic.allocator(),
                                                        "WW.W4", 64),
                            &Magnet_WWW4);
StandardFixedTurnout Turnout_WWW5(&brd, EventBlock::Allocator(logic.allocator(),
                                                              "WW.W5", 40),
                                  FixedTurnout::TURNOUT_CLOSED);

StandardBlock Block_WWA11(&brd, &WWA11, logic.allocator(), "WW.A11");
StubBlock Block_WWB2(&brd, &WWB2, &b6.InOraRed, logic.allocator(), "WW.B2");
StubBlock Block_WWB3(&brd, &WWB3, &b6.InOraGreen, logic.allocator(), "WW.B3");
StandardBlock Block_WWB14(&brd, &WWB14, logic.allocator(), "WW.B14");

StandardBlock Block_A301(&brd, &A301, logic.allocator(), "A301");
StandardBlock Block_A321(&brd, &A321, logic.allocator(), "A321");
StandardBlock Block_A347(&brd, &A347, logic.allocator(), "A347");
StandardBlock Block_A360(&brd, &A360, logic.allocator(), "A360");

StandardBlock Block_B421(&brd, &B421, logic.allocator(), "B421");
StandardBlock Block_B447(&brd, &B447, logic.allocator(), "B447");
StandardBlock Block_B460(&brd, &B460, logic.allocator(), "B460");
StandardBlock Block_B475(&brd, &B475, logic.allocator(), "B475");

StandardFixedTurnout Turnout_W359(&brd, EventBlock::Allocator(logic.allocator(),
                                                              "W359", 40),
                                  FixedTurnout::TURNOUT_THROWN);
StandardFixedTurnout Turnout_W459(&brd, EventBlock::Allocator(logic.allocator(),
                                                              "W459", 40),
                                  FixedTurnout::TURNOUT_THROWN);

MagnetDef Magnet_ZZW1(&g_magnet_aut, "ZZ.W1", &bb.ActGreenGreen,
                      &bb.ActGreenRed);
StandardMovableTurnout Turnout_ZZW1(
    &brd, EventBlock::Allocator(logic.allocator(), "ZZ.W1", 40), &Magnet_ZZW1);

StandardFixedTurnout Turnout_ZZW2(&brd, EventBlock::Allocator(logic.allocator(),
                                                              "ZZ.W2", 40),
                                  FixedTurnout::TURNOUT_THROWN);
MagnetDef Magnet_ZZW3(&g_magnet_aut, "ZZ.W3", &bb.ActBlueGrey,
                      &bb.ActBlueBrown);
StandardMovableDKW DKW_ZZW3(&brd, EventBlock::Allocator(logic.allocator(),
                                                        "ZZ.W3", 64),
                            &Magnet_ZZW3);
StubBlock Block_ZZA2(&brd, &ZZA2, &bb.InOraGreen, logic.allocator(), "ZZ.A2");
StubBlock Block_ZZA3(&brd, &ZZA3, &bb.InOraRed, logic.allocator(), "ZZ.A3");

StandardMiddleDetector Det_380(&brd, &b7.InOraRed,
                               EventBlock::Allocator(logic2.allocator(),
                                                     "Det380", 24, 8));

StandardFixedTurnout Turnout_W381(&brd,
                                  EventBlock::Allocator(logic2.allocator(),
                                                        "W381", 40),
                                  FixedTurnout::TURNOUT_THROWN);
StandardFixedTurnout Turnout_W382(&brd,
                                  EventBlock::Allocator(logic2.allocator(),
                                                        "W382", 40),
                                  FixedTurnout::TURNOUT_CLOSED);

StandardMiddleDetector Det_500(&brd, &b7.InBrownBrown,
                               EventBlock::Allocator(logic2.allocator(),
                                                     "Det500", 24, 8));

MagnetDef Magnet_W481(&g_magnet_aut, "W481", &b7.ActBlueGrey, &b7.ActBlueBrown);
StandardMovableTurnout Turnout_W481(
    &brd, EventBlock::Allocator(logic2.allocator(), "W481", 40), &Magnet_W481);

StandardBlock Block_YYB2(&brd, &YYB2, logic2.allocator(), "YY.B2");
StandardBlock Block_YYA3(&brd, &YYA3, logic2.allocator(), "YY.A3");
StandardBlock Block_YYC23(&brd, &YYC23, logic2.allocator(), "YY.C23");
StandardMiddleDetector Det_YYC22(&brd, &ba.In1,
                                 EventBlock::Allocator(logic2.allocator(),
                                                       "YY.C22", 24, 8));

StandardFixedTurnout Turnout_YYW6(&brd, EventBlock::Allocator(logic2.allocator(),
                                                              "YY.W6", 40),
                                  FixedTurnout::TURNOUT_THROWN);

StandardFixedTurnout Turnout_XXW1(&brd,
                                  EventBlock::Allocator(logic2.allocator(),
                                                        "XX.W1", 40),
                                  FixedTurnout::TURNOUT_THROWN);
StandardFixedTurnout Turnout_XXW2(&brd,
                                  EventBlock::Allocator(logic2.allocator(),
                                                        "XX.W2", 40),
                                  FixedTurnout::TURNOUT_THROWN);
//MagnetDef Magnet_XXW7(&g_magnet_aut, "XX.W7", &b3.ActBlueGrey,
//                      &b3.ActBlueBrown);
StandardFixedTurnout Turnout_XXW7(
    &brd, EventBlock::Allocator(logic2.allocator(), "XX.W7", 40),
    FixedTurnout::TURNOUT_CLOSED);  // we ignore the magnets here
MagnetDef Magnet_XXW8(&g_magnet_aut, "XX.W8", &ba.ActBlueGrey, &ba.ActBlueBrown);
StandardMovableTurnout Turnout_XXW8(
    &brd, EventBlock::Allocator(logic2.allocator(), "XX.W8", 40), &Magnet_XXW8);

StandardBlock Block_XXB1(&brd, &XXB1, logic2.allocator(), "XX.B1");
StandardBlock Block_XXB2(&brd, &XXB2, logic2.allocator(), "XX.B2");
StandardBlock Block_XXB3(&brd, &XXB3, logic2.allocator(), "XX.B3");

bool ignored1 = BindSequence({&Block_A347, &Block_A321, &Block_A301});
bool ignored2 = BindSequence({&Block_B421, &Block_B447, &Block_B460});
/*bool ignored2 = Block_YYC23.side_b()->Bind(Turnout_XXW8.b.side_points());
bool ignored3 = Block_XXB1.side_a()->Bind(Turnout_XXW8.b.side_closed());
bool ignored4 = Block_XXB3.side_a()->Bind(Turnout_XXW8.b.side_thrown());
bool ignored5 = Block_A301.side_a()->Bind(Turnout_XXW1.b.side_points());
bool ignored6 = Block_XXB1.side_b()->Bind(Turnout_XXW1.b.side_closed());
bool ignored7 = Block_XXB3.side_b()->Bind(Turnout_XXW1.b.side_thrown());*/

bool ign =
    BindPairs({{Block_YYC23.side_b(), Turnout_YYW6.b.side_closed()},
               {Det_YYC22.side_b(), Turnout_YYW6.b.side_thrown()},
               {Block_YYB2.side_a(), Det_YYC22.side_a()},
               {Turnout_YYW6.b.side_points(), Turnout_XXW8.b.side_points()},
               {Block_XXB1.side_a(), Turnout_XXW8.b.side_closed()},
               {Turnout_XXW7.b.side_points(), Turnout_XXW8.b.side_thrown()},
               {Block_XXB2.side_b(), Turnout_XXW7.b.side_thrown()},
               {Block_XXB3.side_a(), Turnout_XXW7.b.side_closed()},
               {Block_XXB1.side_b(), Turnout_XXW1.b.side_closed()},
               {Block_XXB2.side_a(), Turnout_XXW2.b.side_thrown()},
               {Block_XXB3.side_b(), Turnout_XXW2.b.side_closed()},
               {Turnout_XXW1.b.side_thrown(), Turnout_XXW2.b.side_points()},
               {Turnout_XXW1.b.side_points(), Det_500.side_a()},
               {Det_500.side_b(), Turnout_W382.b.side_closed()},
               {Turnout_W382.b.side_thrown(), Block_YYB2.side_b()},
               {Turnout_W382.b.side_points(), Turnout_W381.b.side_points()},
               {Turnout_W381.b.side_thrown(), Det_380.side_a()},
               {Det_380.side_b(), Turnout_ZZW2.b.side_thrown()},
               {Turnout_ZZW2.b.side_points(), Block_A360.side_a()},
               {Turnout_ZZW2.b.side_closed(), DKW_ZZW3.b.point_a2()},
               {DKW_ZZW3.b.point_b2(), Block_ZZA2.entry()},
               {Turnout_W381.b.side_closed(), Turnout_W481.b.side_thrown()},
               {Block_YYC23.side_a(), Block_YYA3.side_b()},
               {Block_YYA3.side_a(), Turnout_W481.b.side_closed()},
               {Block_B460.side_b(), Turnout_W459.b.side_closed()},
               {Turnout_W459.b.side_points(), Block_B475.side_a()},
               {Turnout_W459.b.side_thrown(), Turnout_W359.b.side_closed()},
               {Turnout_W359.b.side_thrown(), Block_A360.side_b()},
               {Turnout_W359.b.side_points(), Block_A347.side_a()},
               {Block_B475.side_b(), Turnout_ZZW1.b.side_points()},
               {Turnout_ZZW1.b.side_thrown(), DKW_ZZW3.b.point_a1()},
               {Turnout_ZZW1.b.side_closed(), Block_ZZA3.entry()},
               {DKW_ZZW3.b.point_b1(), Turnout_W481.b.side_points()},
               {Block_A301.side_b(), Turnout_WWW1.b.side_points()},
               {Block_WWB14.side_a(), Block_WWA11.side_b()},
               {Block_WWA11.side_a(), DKW_WWW4.b.point_b1()},
               {DKW_WWW4.b.point_b2(), Block_WWB2.entry()},
               {DKW_WWW4.b.point_a2(), Turnout_WWW1.b.side_thrown()},
               {Turnout_WWW3.b.side_thrown(), DKW_WWW4.b.point_a1()},
               {Turnout_WWW2.b.side_thrown(), Turnout_WWW1.b.side_closed()},
               {Turnout_WWW2.b.side_closed(), Block_B421.side_a()},
               {Turnout_WWW2.b.side_points(), Turnout_WWW3.b.side_points()},
               {Turnout_WWW3.b.side_closed(), Turnout_WWW5.b.side_points()},
               {Turnout_WWW5.b.side_closed(), Block_WWB3.entry()},
               {Turnout_WWW5.b.side_thrown(), Block_WWB14.side_b()},
                   });

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

DefAut(signalaut1, brd, {
    BlockSignal(this, &Block_A360);
    BlockSignal(this, &Block_A347);
    BlockSignal(this, &Block_A321);
    BlockSignal(this, &Block_A301);
  });

DefAut(signalaut2, brd, {
    BlockSignal(this, &Block_B421);
    BlockSignal(this, &Block_B447);
    BlockSignal(this, &Block_B475);
  });

FlipFlopAutomata loop_flipflop(&brd, "loop_flipflop", *logic.allocator(), 32);
FlipFlopClient lpc_tofront1("to_front_1", &loop_flipflop);
FlipFlopClient lpc_toback1("to_back_1", &loop_flipflop);
FlipFlopClient lpc_tofront3("to_front_3", &loop_flipflop);
FlipFlopClient lpc_toback1x("to_back_1x", &loop_flipflop);

FlipFlopAutomata front_flipflop(&brd, "front_flipflop", *logic.allocator(), 32);
FlipFlopClient frc_tofront("to_front", &front_flipflop);
FlipFlopClient frc_fromfront1("from_front1", &front_flipflop);
FlipFlopClient frc_toback("to_back", &front_flipflop);
FlipFlopClient frc_fromback("from_back", &front_flipflop);
FlipFlopClient frc_fromfront3("from_front3", &front_flipflop);

FlipFlopAutomata ww_in_flipflop(&brd, "ww_in_flipflop", *logic.allocator(), 32);
FlipFlopClient ww_to2("to_2", &ww_in_flipflop);
FlipFlopClient ww_to3("to_3", &ww_in_flipflop);

FlipFlopAutomata ww_out_flipflop(&brd, "ww_out_flipflop", *logic.allocator(),
                                 32);
FlipFlopClient ww_from2("from_2", &ww_out_flipflop);
FlipFlopClient ww_from3("from_3", &ww_out_flipflop);
FlipFlopClient ww_from14("from_14", &ww_out_flipflop);


std::unique_ptr<GlobalVariable> g_stop_b460(logic2.allocator()->Allocate("block_b460"));
std::unique_ptr<GlobalVariable> g_stop_b360(logic2.allocator()->Allocate("block_b360"));

void IfLoopOkay(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW8.b.any_route()));
}

// B475->XXB2
void IfFrontFrontInOkay(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_W381.b.any_route()))
      .IfReg0(op->parent()->ImportVariable(*Turnout_XXW1.b.any_route()))
      .IfReg0(op->parent()->ImportVariable(*DKW_ZZW3.b.any_route()));
}

// XXB1/XXB3 -> A360
void IfFrontFrontOutOkay(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_W381.b.any_route()))
      .IfReg0(op->parent()->ImportVariable(*Turnout_XXW1.b.any_route()));
}

// YYC2 -> A360
void IfFrontBackOutOkay(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_W381.b.any_route()));
}

void IfZZW3Free(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*DKW_ZZW3.b.any_route()));
}

void IfZZW1Free(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_ZZW1.b.any_route()));
}

void IfZZ2OutFree(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_ZZW1.b.any_route()))
      .IfReg0(op->parent()->ImportVariable(Block_B475.detector()))
      .IfReg0(op->parent()->ImportVariable(Block_B475.route_in()))
      .IfReg0(op->parent()->ImportVariable(*Turnout_W359.b.any_route()));
}

void IfWWB3EntryFree(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_WWW2.b.any_route()));
}

void IfWWB2EntryFree(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*DKW_WWW4.b.any_route()));
}

void IfWWB2ExitFree(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*DKW_WWW4.b.any_route()))
      .IfReg0(op->parent()->ImportVariable(*Turnout_WWW2.b.any_route()));
}

void IfB460OutNotBlocked(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*g_stop_b460));
}

void IfB360OutNotBlocked(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*g_stop_b360));
}

auto g_b460_not_blocked = NewCallback(&IfB460OutNotBlocked);
auto g_b360_not_blocked = NewCallback(&IfB360OutNotBlocked);
auto g_wwb3_entry_free = NewCallback(&IfWWB3EntryFree);
auto g_wwb2_entry_free = NewCallback(&IfWWB2EntryFree);
auto g_wwb2_exit_free = NewCallback(&IfWWB2ExitFree);
auto g_zzw3_free = NewCallback(&IfZZW3Free);
auto g_zzw1_free = NewCallback(&IfZZW1Free);
auto g_zz2_out_free = NewCallback(&IfZZ2OutFree);
auto g_loop_condition = NewCallback(&IfLoopOkay);
auto g_front_front_in_condition = NewCallback(&IfFrontFrontInOkay);
auto g_front_front_out_condition = NewCallback(&IfFrontFrontOutOkay);
auto g_front_back_out_condition = NewCallback(&IfFrontBackOutOkay);

class LayoutSchedule : public TrainSchedule {
 public:
  LayoutSchedule(const string& name, uint16_t train_id, uint8_t default_speed)
      : TrainSchedule(name, &brd, NODE_ID_DCC | train_id,
                      train_perm.Allocate(name, 24, 8),
                      train_tmp.Allocate(name, 24, 8),
                      &stored_speed_),
        stored_speed_(&brd, "speed." + name,
                      BRACZ_SPEEDS | ((train_id & 0xff) << 8), default_speed) {}

 protected:
  // Runs down from ZZ to WW on track 300.
  void Run360_to_301(Automata* aut) {
    AddEagerBlockTransition(&Block_A360, &Block_A347, &g_b360_not_blocked);
    AddEagerBlockSequence({&Block_A347, &Block_A321, &Block_A301},
                          &g_not_paused_condition);
  }

  // In WW, runs around the loop track 11 to 14.
  void RunLoopWW(Automata* aut) {
    AddEagerBlockTransition(&Block_A301, &Block_WWA11, &g_wwb2_entry_free);
    SwitchTurnout(Turnout_WWW1.b.magnet(), true);
    SwitchTurnout(DKW_WWW4.b.magnet(), true);
    AddEagerBlockTransition(&Block_WWA11, &Block_WWB14, &g_not_paused_condition);

    AddBlockTransitionOnPermit(&Block_WWB14, &Block_B421, &ww_from14,
                               &g_wwb3_entry_free);
  }

  // In WW, run through the stub track, changing direction.
  void RunStubWW(Automata* aut) {
    AddBlockTransitionOnPermit(&Block_A301, &Block_WWB3.b_, &ww_to3, &g_wwb3_entry_free);
    SwitchTurnout(Turnout_WWW1.b.magnet(), false);
    StopAndReverseAtStub(&Block_WWB3);

    AddBlockTransitionOnPermit(&Block_A301, &Block_WWB2.b_, &ww_to2, &g_wwb2_entry_free);
    SwitchTurnout(Turnout_WWW1.b.magnet(), true);
    SwitchTurnout(DKW_WWW4.b.magnet(), false);
    StopAndReverseAtStub(&Block_WWB2);

    AddBlockTransitionOnPermit(&Block_WWB3.b_, &Block_B421, &ww_from3,
                               &g_wwb3_entry_free);
    AddBlockTransitionOnPermit(&Block_WWB2.b_, &Block_B421, &ww_from2,
                               &g_wwb2_exit_free);
    SwitchTurnout(DKW_WWW4.b.magnet(), true);
  }

  // In WW, run through the stub track, changing direction.
  void RunStubWWB3(Automata* aut) {
    AddBlockTransitionOnPermit(&Block_A301, &Block_WWB3.b_, &ww_to3, &g_wwb3_entry_free);
    SwitchTurnout(Turnout_WWW1.b.magnet(), false);
    StopAndReverseAtStub(&Block_WWB3);

    AddBlockTransitionOnPermit(&Block_WWB3.b_, &Block_B421, &ww_from3,
                               &g_wwb3_entry_free);
  }

  // Runs up from WW to ZZ on track 400.
  void Run421_to_475(Automata* aut) {
    AddEagerBlockTransition(&Block_B421, &Block_B447, &g_not_paused_condition);
    AddEagerBlockTransition(&Block_B447, &Block_B460, &g_not_paused_condition);
    AddEagerBlockTransition(&Block_B460, &Block_B475, &g_b460_not_blocked);
  }

  // Runs in ZZ into the stub track and reverses direction.
  void RunStubZZ(Automata* aut) {
    AddEagerBlockTransition(&Block_B475, &Block_ZZA2.b_, &g_zzw3_free);
    SwitchTurnout(DKW_ZZW3.b.magnet(), true);
    SwitchTurnout(Turnout_ZZW1.b.magnet(), true);
    StopAndReverseAtStub(&Block_ZZA2);

    AddEagerBlockTransition(&Block_ZZA2.b_, &Block_A360, &g_zzw3_free);
    SwitchTurnout(DKW_ZZW3.b.magnet(), false);
  }

  void RunStub2ZZ(Automata* aut) {
    AddEagerBlockTransition(&Block_B475, &Block_ZZA3.b_, &g_zzw1_free);
    SwitchTurnout(Turnout_ZZW1.b.magnet(), false);
    StopAndReverseAtStub(&Block_ZZA3);

    AddEagerBlockTransition(&Block_ZZA3.b_, &Block_A347, &g_zz2_out_free);
    auto* stop_b460 = aut->ImportVariable(g_stop_b460.get());
    auto* stop_b360 = aut->ImportVariable(g_stop_b360.get());
    Def().IfReg0(current_block_permaloc_)
        .ActReg0(stop_b460)
        .ActReg0(stop_b360);
    Def().IfReg1(current_block_permaloc_)
        .IfState(StWaiting)
        .ActReg1(stop_b460);
    Def().IfReg1(current_block_permaloc_)
        .IfState(StWaiting)
        .IfReg1(aut->ImportVariable(Block_B475.detector()))
        .ActReg0(stop_b360);
    Def().IfReg1(current_block_permaloc_)
        .IfState(StWaiting)
        .IfReg1(aut->ImportVariable(Block_B475.route_in()))
        .ActReg0(stop_b360);
    Def().IfReg1(current_block_permaloc_)
        .IfState(StWaiting)
        .IfReg0(aut->ImportVariable(Block_B475.route_in()))
        .IfReg0(aut->ImportVariable(Block_B475.detector()))
        .ActReg1(stop_b360);
  }

  // Runs the loop XX/YY from 475 to eventually 360
  void RunLoopXXYY(Automata* aut) {
    // in
    AddBlockTransitionOnPermit(&Block_B475, &Block_YYA3, &frc_toback, &g_zzw3_free);
    SwitchTurnout(Turnout_W481.b.magnet(), false);
    SwitchTurnout(DKW_ZZW3.b.magnet(), false);
    SwitchTurnout(Turnout_ZZW1.b.magnet(), true);
    AddBlockTransitionOnPermit(&Block_B475, &Block_XXB2, &frc_tofront, &g_front_front_in_condition);
    SwitchTurnout(Turnout_W481.b.magnet(), true);
    SwitchTurnout(DKW_ZZW3.b.magnet(), false);
    SwitchTurnout(Turnout_ZZW1.b.magnet(), true);

    AddEagerBlockTransition(&Block_YYA3, &Block_YYC23, &g_not_paused_condition);

    // back->front
    AddBlockTransitionOnPermit(&Block_YYC23, &Block_XXB3, &lpc_tofront3,
                               &g_loop_condition);
    SwitchTurnout(Turnout_XXW8.b.magnet(), true);
    AddBlockTransitionOnPermit(&Block_YYC23, &Block_XXB1, &lpc_tofront1,
                               &g_loop_condition);
    SwitchTurnout(Turnout_XXW8.b.magnet(), false);

    // front->back
    AddBlockTransitionOnPermit(&Block_XXB2, &Block_YYB2, &lpc_toback1,
                               &g_loop_condition);
    AddBlockTransitionOnPermit(&Block_XXB2, &Block_YYB2, &lpc_toback1x,
                               &g_loop_condition);

    // out
    AddBlockTransitionOnPermit(&Block_XXB1, &Block_A360, &frc_fromfront1, &g_front_front_out_condition);
    AddBlockTransitionOnPermit(&Block_XXB3, &Block_A360, &frc_fromfront3, &g_front_front_out_condition);
    AddBlockTransitionOnPermit(&Block_YYB2, &Block_A360, &frc_fromback, &g_front_back_out_condition);
  }

  ByteImportVariable stored_speed_;
};


class CircleTrain : public LayoutSchedule {
 public:
  CircleTrain(const string& name, uint16_t train_id, uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    Run360_to_301(aut);
    RunLoopWW(aut);
    Run421_to_475(aut);
    RunLoopXXYY(aut);
  }
};

class EWIVPendelzug : public LayoutSchedule {
 public:
  EWIVPendelzug(const string& name, uint16_t train_id, uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    Run360_to_301(aut);
    RunStubWW(aut);
    Run421_to_475(aut);
    RunLoopXXYY(aut);
  }
};

class StraightOnlyPushPull : public LayoutSchedule {
 public:
  StraightOnlyPushPull(const string& name, uint16_t train_id,
                       uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    Run360_to_301(aut);
    RunStubWW(aut);
    Run421_to_475(aut);
    RunStubZZ(aut);
  }
};

class M61PushPull : public LayoutSchedule {
 public:
  M61PushPull(const string& name, uint16_t train_id,
              uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    Run360_to_301(aut);
    RunStubWWB3(aut);
    Run421_to_475(aut);
    RunStubZZ(aut);
  }
};

class ICEPushPull : public LayoutSchedule {
 public:
  ICEPushPull(const string& name, uint16_t train_id, uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    Run360_to_301(aut);
    RunStubWW(aut);
    Run421_to_475(aut);
    RunStub2ZZ(aut);
  }
};

StraightOnlyPushPull train_icn("icn", 50, 16);
CircleTrain train_re66("re_6_6", 66, 32);
CircleTrain train_rts("rts_railtraction", 32, 20);
CircleTrain train_re460hag("Re460_HAG", 26, 32);
CircleTrain train_re465("Re465", 47, 32);
EWIVPendelzug train_ewivpendelzug("Re460TSR", 22, 20);
CircleTrain train_rbe44("RBe4_4_ZVV", 52, 35);
M61PushPull train_m61("m61", 61, 30);
CircleTrain train_11239("Re44_11239", 48, 28);
ICEPushPull train_ice("ICE", 2, 16);
CircleTrain train_wle("wle_er20", 27, 30);
CircleTrain train_re474("Re474", 12, 30);
CircleTrain train_krokodil("Krokodil", 68, 35);
CircleTrain train_rheingold("Rheingold", 19, 35);
CircleTrain train_re10_10("Re_10_10", 3, 35);

int main(int argc, char** argv) {
  automata::reset_routes = &reset_all_routes;

  FILE* f = fopen("automata.bin", "wb");
  assert(f);
  string output;
  brd.Render(&output);
  fwrite(output.data(), 1, output.size(), f);
  fclose(f);

  f = fopen("bracz-layout3h-logic.cout", "wb");
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
  map<uint64_t, string>& m(*automata::GetEventMap());
  for (const auto& it : m) {
    fprintf(f, "%016llx: %s\n", it.first, it.second.c_str());
  }
  fclose(f);

  f = fopen("debug.txt", "w");
  fwrite(GetDebugData()->data(), GetDebugData()->size(), 1, f);
  fclose(f);

  f = fopen("jmri-out.xml", "w");
  assert(f);
  PrintAllEventVariables(f);
  fclose(f);

  f = fopen("var-bash-list.txt", "w");
  assert(f);
  PrintAllEventVariablesInBashFormat(f);
  fclose(f);

  fprintf(stderr, "Allocator %s: %d entries remaining\n",
          logic.allocator()->name().c_str(), logic.allocator()->remaining());
  fprintf(stderr, "Allocator %s: %d entries remaining\n",
          logic2.allocator()->name().c_str(), logic2.allocator()->remaining());
  fprintf(stderr, "Allocator %s: %d entries remaining\n",
          perm.allocator()->name().c_str(), perm.allocator()->remaining());

  return 0;
};
