#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>

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

EventBasedVariable is_shutdown(&brd, "is_shutdown",
                               BRACZ_LAYOUT | 0x000c, BRACZ_LAYOUT | 0x000d, 7,
                               31, 4);

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

EventBasedVariable watchdog(&brd, "reset_watchdog", BRACZ_LAYOUT | 0x0010,
                            BRACZ_LAYOUT | 0x0010, 7, 30, 5);

EventBasedVariable reset_all_routes(&brd, "reset_routes", BRACZ_LAYOUT | 0x0012,
                                    BRACZ_LAYOUT | 0x0013, 7, 30, 4);

EventBasedVariable route_lock_ZH(&brd, "route_lock_ZH", BRACZ_LAYOUT | 0x0014,
                                 BRACZ_LAYOUT | 0x0015, 7, 30, 3);

EventBasedVariable route_lock_BS(&brd, "route_lock_BS", BRACZ_LAYOUT | 0x0016,
                                 BRACZ_LAYOUT | 0x0017, 7, 30, 2);


EventBasedVariable estop_short(&brd, "estop_short", BRACZ_LAYOUT | 0x0018,
                               BRACZ_LAYOUT | 0x0019, 7, 30, 1);

EventBasedVariable estop_nopwr(&brd, "estop", 0x010000000000FFFFULL,
                               0x010000000000FFFEULL, 7, 30, 0);

EventBasedVariable global_dispatch(&brd, "global_dispatch",
                                   BRACZ_LAYOUT | 0x001A, BRACZ_LAYOUT | 0x001B,
                                   7, 29, 7);

constexpr auto SCENE_OFF_EVENT = BRACZ_LAYOUT | 0x0029;


// I2CBoard b5(0x25), b6(0x26); //, b7(0x27), b1(0x21), b2(0x22);
//NativeIO n9(0x29);
AccBoard bd(0x2d), b9(0x29), ba(0x2a);

/*StateRef StGreen(2);
StateRef StGoing(3);
*/

StateRef StUser1(10);
StateRef StUser2(11);



/* More signals

address 158 (0x9E) main 26 adv 27  reflashed in YYC23
address 155 (0x9B) main 6 adv 7   reflashed in B475
address 145 (0x91) main 32 adv 33 reflashed in B375

address 129 (0x81) adv 74 reflashed in A475
address 149 (0x95) adv 75 reflashed in A375

address 142 (0x8E) main 12 adv 13 reflashed in A460
address 139 (0x8B) main 14 adv 15 reflashed in A360
address 144 (0x90) adv 19 reflashed in B460
address 138 (0x8A) adv 17 reflashed in B360
address 131 (0x83) main 10 adv 11 reflashed in B447


address 166 (0xA6) main 36 adv 37 reflashed in A321
address 172 (0xAC) main 38 adv 39 reflashed in B321
address 176 (0xB0) main 40 adv 41 reflashed in B421
address 134 (0x86) main 42 adv 43 reflashed in A421

address 147 (0x93) main 25 reflashed in XX.B1
address 140 (0x8C) main 24 reflashed in XX.B3

address 141 (0x8D) main 4 adv 5 reflashed in YY.B2

address 157 (0x9D) main 22 adv 23 reflashed in A347
address 159 (0x9F) main 31 adv 32 reflashed in XX.B2

address 151 (0x97) main 44 adv 45 reflshed in A447
address 140 main 34 adv 35 in A401 reflashed

address 191 (0xBF) main 63 in B460-main reflashed
address 174 (0xAE) main 62 in B360-main reflashed
address 143 (0x8F) main 8 adv 9 reflashed in B347

address 173  main 69 adv 70 in WW.B14 reflashed
address 167  main 67 adv 68 in WW.B3 reflashed
address 171  main 51 adv 52 in WW.B2 reflashed
address 164  main 49 adv 50 in WW.B11 reflashed

address 133 0x85 main 53 adv 54 reflashed in A380
address 130 0x82 main 55 adv 56 reflashed in A480
address 153 0x99 main 57 adv 58 reflashed in ZZA2
address 136 0x86 main 65 adv 66 reflashed in ZZA3

 I2CSignal signal_WWB3_main(&b5, 75, "WW.B3.main");  // todo:
 I2CSignal signal_WWB3_adv(&b5, 74, "WW.B3.adv");

*/


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
unique_ptr<GlobalVariable> acc_off_tmp(NewTempVariable(&brd));

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
    Def().IfReg1(*power_acc).IfState(StPaused).ActState(StBase);
  }

  GlobalVariable* power_magnets_;
};

/// Set this to true if Acc power exists in this layout.
static const bool kHaveAccPower = false;

DefAut(watchdog, brd, {
  LocalVariable* w = ImportVariable(&watchdog);
  Def().IfState(StInit).ActReg1(ImportVariable(&is_paused)).ActState(StBase);
  Def().IfTimerDone().ActReg1(w).ActReg0(w).ActTimer(1);

  auto* signal_off_tmp = ImportVariable(acc_off_tmp.get());
  auto* signal_on = ImportVariable(&power_acc);
  const auto& signal_short = ImportVariable(short_det);
  const auto& signal_over = ImportVariable(overcur);
  Def().IfReg1(*signal_on).ActReg0(signal_off_tmp);
  Def().IfReg1(signal_short).ActReg0(signal_off_tmp);
  Def().IfReg1(signal_over).ActReg0(signal_off_tmp);

  // If the signal power is off but no short and no overcurrent detected,
  // turn it back on.
  Def().IfReg0(*signal_on).IfReg1(*signal_off_tmp).ActReg1(signal_on);

  // But do this only after waiting for one cycle for any overcurrent event
  // to come in.
  Def()
      .IfReg0(*signal_on)
      .IfReg0(signal_short)
      .IfReg0(signal_over)
      .ActReg1(signal_off_tmp);

  if (kHaveAccPower) {
    // Turns on is_paused when the accessory bus turned off due to short etc.
    auto* lis_paused = ImportVariable(&is_paused);
    Def().IfReg1(signal_short).ActReg1(lis_paused);
    Def().IfReg1(signal_over).ActReg1(lis_paused);
  }
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

  DefCopy(*rep, ImportVariable(&b9.LedBlueSw));
  DefCopy(*rep, ImportVariable(&ba.LedBlueSw));
  DefCopy(*rep, ImportVariable(&bd.LedBlueSw));
});

/*DefAut(testaut, brd, { Def().IfState(StInit).ActState(StBase);
    DefCopy(ImportVariable(bc.In0), ImportVariable(&bc.Act0));
    DefCopy(ImportVariable(bc.In1), ImportVariable(&bc.Act1));
    DefCopy(ImportVariable(bc.In2), ImportVariable(&bc.Act2));
    DefCopy(ImportVariable(bc.In3), ImportVariable(&bc.Act3));
    DefCopy(ImportVariable(bc.In4), ImportVariable(&bc.Act4));
    DefCopy(ImportVariable(bc.In5), ImportVariable(&bc.Act5));
    DefCopy(ImportVariable(bc.In6), ImportVariable(&bc.Act6));
    DefCopy(ImportVariable(bc.In7), ImportVariable(&bc.Act7));
    });*/


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

void IfNotShutdown(Automata::Op* op) {
  op->IfReg0(op->parent()->ImportVariable(is_shutdown));
}


namespace automata {
CallbackSpec1_0<Automata::Op*> g_not_paused_condition = NewCallback(&IfNotPaused);
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

class MagnetButtonAutomata {
 public:
  MagnetButtonAutomata(Board* brd) : aut_("magnetbutton", brd, &plugins_) {}

  void Register(AutomataCallback* plugin) {
    plugins_.AddAutomataPlugin(ofs_++, plugin);
  }

 private:
  unsigned ofs_ = 100;
  AutomataPlugin plugins_;
  StandardPluginAutomata aut_;
};

class MagnetButton {
 public:
  MagnetButton(MagnetButtonAutomata* parent, const AllocatorPtr& parent_alloc,
               MagnetDef* magnet, const GlobalVariable& button)
      : alloc_(parent_alloc->Allocate(string("btn.") + magnet->name_, 4)),
        button_(button),
        magnet_(magnet) {
    parent->Register(NewCallbackPtr(this, &MagnetButton::Run));
  }

 private:
  AllocatorPtr alloc_;
  const GlobalVariable& button_;
  MagnetDef* magnet_;
  std::unique_ptr<GlobalVariable> shadow_{alloc_->Allocate("shadow")};
  std::unique_ptr<GlobalVariable> armed_{alloc_->Allocate("armed")};
  void Run(Automata* aut);
};

void MagnetButton::Run(Automata* aut) {
  aut->ClearUsedVariables();
  const Automata::LocalVariable& btn = aut->ImportVariable(button_);
  Automata::LocalVariable* shadow = aut->ImportVariable(shadow_.get());
  Automata::LocalVariable* armed = aut->ImportVariable(armed_.get());
  Automata::LocalVariable* cmd = aut->ImportVariable(magnet_->command.get());
  Def().IfState(StInit).ActReg0(shadow).ActReg0(armed);

  // Flip the state if the button is just pressed.
  Def()
      .IfReg0(btn)
      .IfReg1(*shadow)
      .IfReg0(btn)
      .IfReg0(*cmd)
      .ActReg1(cmd)
      .ActReg0(shadow);
  Def()
      .IfReg0(btn)
      .IfReg1(*shadow)
      .IfReg0(btn)
      .IfReg1(*cmd)
      .ActReg0(cmd)
      .ActReg0(shadow);

  // Arm the timer when the button is released.
  Def().IfReg0(*shadow).IfReg1(btn).IfReg0(*armed).ActTimer(1).ActReg1(armed);
  // When the timer expires with the button up, reset shadow.
  Def()
      .IfReg0(*shadow)
      .IfReg1(btn)
      .IfReg1(*armed)
      .IfTimerDone()
      .ActReg1(shadow)
      .ActReg0(armed);
  // Otherwise if we see a bounce, go back with the arming.
  Def()
      .IfReg0(*shadow)
      .IfReg0(btn)
      .IfReg1(*armed)
      .ActReg0(armed);
}

EventBlock perm(&brd, BRACZ_LAYOUT | 0xC000, "perm", 1024 / 2);

EventBlock l1(&brd, BRACZ_LAYOUT | 0xD000, "logic");
EventBlock l2(&brd, BRACZ_LAYOUT | 0xE000, "logic");
EventBlock l3(&brd, BRACZ_LAYOUT | 0xF000, "logic");
AllocatorPtr logic(new UnionAllocator({l1.allocator().get(), l2.allocator().get(), l3.allocator().get()}));
const AllocatorPtr& logic2(logic);
const AllocatorPtr& train_perm(perm.allocator());

AllocatorPtr train_tmp(logic2->Allocate("train", 768));

MagnetCommandAutomata g_magnet_aut(&brd, logic2);
MagnetPause magnet_pause(&g_magnet_aut, &power_acc);
MagnetButtonAutomata g_btn_aut(&brd);


/*
MagnetDef Magnet_XXW1(&g_magnet_aut, "XX.W1", &bb.ActBrownGrey,
                      &bb.ActBrownBrown, MovableTurnout::kThrown);
StandardMovableTurnout Turnout_XXW1(&brd,
                                    logic->Allocate("XX.W1", 40),
                                    &Magnet_XXW1);
TurnoutWrap TXXW1(&Turnout_XXW1.b, kThrownToPoint);

CoupledMagnetDef Magnet_XXW2(&g_magnet_aut, "XX.W2", &Magnet_XXW1, true);
StandardMovableTurnout Turnout_XXW2(&brd,
                                    logic->Allocate("XX.W2", 40),
                                    &Magnet_XXW2);
TurnoutWrap TXXW2(&Turnout_XXW2.b, kClosedToPoint);
*/

/*
PhysicalSignal QQA1(&b9.In6, nullptr, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr);
PhysicalSignal QQA2(&b9.In7, nullptr, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr);
PhysicalSignal QQB3(&b9.InOraGreen, nullptr, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr);

PhysicalSignal YYA4(&ba.InGreenYellow, nullptr, nullptr, nullptr,
                    &signal_YYB4_main.signal, &signal_YYB4_adv.signal, nullptr,
                    nullptr);
PhysicalSignal YYA3(&ba.InGreenGreen, nullptr, nullptr, nullptr,
                    &signal_YYB3_main.signal, &signal_YYB3_adv.signal, nullptr,
                    nullptr);
PhysicalSignal YYA2(&ba.InBrownGrey, nullptr, nullptr, nullptr,
                    &signal_YYB2_main.signal, &signal_YYB2_adv.signal, nullptr,
                    nullptr);
PhysicalSignal YYA1(&ba.InBrownBrown, nullptr, nullptr, nullptr,
                    &signal_YYB1_main.signal, &signal_YYB1_adv.signal, nullptr,
                    nullptr);
*/

// 4-way bridge. Heads are from left to right.
I2CSignal signal_ZHB1_main(&b9, 53, "ZH.B1.main");
I2CSignal signal_ZHB1_adv(&b9, 54, "ZH.B1.adv");
I2CSignal signal_ZHB2_main(&b9, 55, "ZH.B2.main");
I2CSignal signal_ZHB2_adv(&b9, 56, "ZH.B2.adv");
I2CSignal signal_ZHB3_main(&b9, 57, "ZH.B3.main");
I2CSignal signal_ZHB3_adv(&b9, 58, "ZH.B3.adv");
I2CSignal signal_ZHB4_main(&b9, 65, "ZH.B4.main");
I2CSignal signal_ZHB4_adv(&b9, 66, "ZH.B4.adv");
// end four-way bridge.

#if 0

// 2-way bridge with main signals on one side, advance on the other. Heads are
// from left to right (starting at main head).
I2CSignal signal_BSA2_main(&bb, 6, "BSA2.main");
I2CSignal signal_BSA2_adv(&bb, 7, "BSA2.adv");
I2CSignal signal_BSA1_main(&bb, 32, "BSA1.main");
I2CSignal signal_BSA1_adv(&bb, 33, "BSA1.adv");
I2CSignal signal_BSB1_adv(&bb, 75, "BSB1.adv");
I2CSignal signal_BSB2_adv(&bb, 74, "BSB2.adv");
// End bridge

#endif


PhysicalSignal ZHA1(&ba.InOraRed, nullptr, nullptr, nullptr,
                    &signal_ZHB1_main.signal, &signal_ZHB1_adv.signal, nullptr,
                    nullptr);

PhysicalSignal ZHA2(&ba.InOraGreen, nullptr, nullptr, nullptr,
                    &signal_ZHB2_main.signal, &signal_ZHB2_adv.signal, nullptr,
                    nullptr);

PhysicalSignal XXB1(&b9.InBrownBrown, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr);

PhysicalSignal XXA2(&bd.InBrownGrey, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr);

PhysicalSignal YYA2(&b9.InOraGreen, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr);

PhysicalSignal YYB1(&bd.InGreenYellow, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr);

/*
StubBlock Stub_ZHA1(&brd, &ZHA1, nullptr, logic, "ZH.A1");
StubBlock Stub_ZHA2(&brd, &ZHA2, nullptr, logic, "ZH.A2");

StandardFixedTurnout Turnout_ZHW1(&brd, logic->Allocate("ZH.W1", 40),
                                  FixedTurnout::TURNOUT_THROWN);
TurnoutWrap TZHW1(&Turnout_ZHW1.b, kPointToThrown);

*/

StandardFixedTurnout Turnout_YYW1(&brd, logic->Allocate("YY.W1", 40),
                                  FixedTurnout::TURNOUT_THROWN);
TurnoutWrap TYYW1(&Turnout_YYW1.b, kClosedToPoint);

StandardBlock Block_YYB1(&brd, &YYB1, logic, "YY.B1");
StandardBlock Block_YYA2(&brd, &YYA2, logic, "YY.A2");

StandardFixedTurnout Turnout_YYW2(&brd, logic->Allocate("YY.W2", 40),
                                  FixedTurnout::TURNOUT_CLOSED);
TurnoutWrap TYYW2(&Turnout_YYW2.b, kPointToClosed);

StandardFixedTurnout Turnout_XXW1(&brd, logic->Allocate("XX.W1", 40),
                                  FixedTurnout::TURNOUT_THROWN);
TurnoutWrap TXXW1(&Turnout_XXW1.b, kClosedToPoint);

StandardBlock Block_XXB1(&brd, &XXB1, logic, "XX.B1");
StandardBlock Block_XXA2(&brd, &XXA2, logic, "XX.A2");

StandardFixedTurnout Turnout_XXW2(&brd, logic->Allocate("XX.W2", 40),
                                  FixedTurnout::TURNOUT_CLOSED);
TurnoutWrap TXXW2(&Turnout_XXW2.b, kPointToClosed);

bool ignored2 = BindSequence(                            //
    Turnout_YYW1.b.side_points(),                        //
    {&TXXW2, &Block_XXA2, &TXXW1, &TYYW2, &Block_YYA2},  //
    Turnout_YYW1.b.side_closed());

bool ignored1 = BindSequence(      //
    Turnout_YYW1.b.side_thrown(),  //
    {&Block_YYB1},                 //
    Turnout_YYW2.b.side_thrown());

bool ignored3 = BindSequence(      //
    Turnout_XXW1.b.side_thrown(),  //
    {&Block_XXB1},                 //
    Turnout_XXW2.b.side_thrown());

/*
StandardFixedTurnout Turnout_YYW9(&brd, logic->Allocate("YY.W9", 40),
                                  FixedTurnout::TURNOUT_CLOSED);
TurnoutWrap TYYW9(&Turnout_YYW9.b, kThrownToPoint);
*/

void RgSignal(Automata* aut, const Automata::LocalVariable& route_set,
              Automata::LocalVariable* signal) {
  Def().IfReg0(route_set).ActSetValue(signal, 1, A_STOP);
  Def().IfReg1(route_set).ActSetValue(signal, 1, A_90);
}

void RedSignal(Automata* aut, Automata::LocalVariable* signal) {
  Def().ActSetValue(signal, 1, A_STOP);
}

void BlockSignalDir(Automata* aut, CtrlTrackInterface* side_front,
                    CtrlTrackInterface* side_back, SignalVariable* main_sgn,
                    SignalVariable* adv_sgn, SignalVariable* in_adv_sgn,
                    const GlobalVariable& runaround,
                    const Automata::LocalVariable& green) {
  const auto& disp = aut->ImportVariable(global_dispatch);
  const auto& local_ra = aut->ImportVariable(runaround);
  if (main_sgn) {
    auto* sgn = aut->ImportVariable(main_sgn);
    Def().IfReg1(disp).IfReg0(green).ActSetValue(sgn, 1, A_SHUNT);
    Def().IfReg1(local_ra).IfReg0(green).ActSetValue(sgn, 1, A_SHUNT);
    Def().IfReg0(disp).IfReg0(local_ra).IfReg0(green).ActSetValue(sgn, 1, A_STOP);
    const auto& sg1 = aut->ImportVariable(*side_front->in_next_signal_1);
    const auto& sg2 = aut->ImportVariable(*side_front->in_next_signal_2);
    Def().IfReg1(green).IfReg0(sg2).IfReg0(sg1).ActSetValue(sgn, 1, A_40);
    Def().IfReg1(green).IfReg0(sg2).IfReg1(sg1).ActSetValue(sgn, 1, A_60);
    Def().IfReg1(green).IfReg1(sg2).IfReg0(sg1).ActSetValue(sgn, 1, A_90);
    Def().IfReg1(green).IfReg1(sg2).IfReg1(sg1).ActSetValue(sgn, 1, A_GO);
  }
  if (in_adv_sgn) {
    auto* sgn = aut->ImportVariable(in_adv_sgn);
    Def().IfReg0(green).ActSetValue(sgn, 1, A_STOP);
    const auto& sg1 =
        aut->ImportVariable(*side_back->binding()->in_next_signal_1);
    const auto& sg2 =
        aut->ImportVariable(*side_back->binding()->in_next_signal_2);
    Def().IfReg1(green).IfReg0(sg2).IfReg1(sg1).ActSetValue(sgn, 1, A_40);
    Def().IfReg1(green).IfReg1(sg2).IfReg0(sg1).ActSetValue(sgn, 1, A_60);
    Def().IfReg1(green).IfReg1(sg2).IfReg1(sg1).ActSetValue(sgn, 1, A_90);
  }
  if (adv_sgn) {
    auto* sgn = aut->ImportVariable(adv_sgn);
    const auto& sg1 = aut->ImportVariable(*side_front->in_next_signal_1);
    const auto& sg2 = aut->ImportVariable(*side_front->in_next_signal_2);
    Def().IfReg1(disp).IfReg0(green).ActSetValue(sgn, 1, A_SHUNT);
    Def().IfReg1(local_ra).IfReg0(green).ActSetValue(sgn, 1, A_SHUNT);
    Def().IfReg0(disp).IfReg0(local_ra).IfReg0(green).ActSetValue(sgn, 1, A_STOP);
    Def().IfReg1(green).IfReg0(sg2).IfReg0(sg1).ActSetValue(sgn, 1, A_STOP);
    Def().IfReg1(green).IfReg0(sg2).IfReg1(sg1).ActSetValue(sgn, 1, A_40);
    Def().IfReg1(green).IfReg1(sg2).IfReg0(sg1).ActSetValue(sgn, 1, A_60);
    Def().IfReg1(green).IfReg1(sg2).IfReg1(sg1).ActSetValue(sgn, 1, A_90);
  }
}

void BlockSignal(Automata* aut, StandardBlock* block,
                 const GlobalVariable& runaround,
                 const GlobalVariable* rev_runaround = nullptr) {
  if (block->p()->main_sgn || block->p()->adv_sgn || block->p()->in_adv_sgn) {
    BlockSignalDir(aut, block->side_b(), block->side_a(), block->p()->main_sgn,
                   block->p()->adv_sgn, block->p()->in_adv_sgn, runaround,
                   aut->ImportVariable(block->route_out()));
  }
  if (block->p()->r_main_sgn || block->p()->r_adv_sgn ||
      block->p()->r_in_adv_sgn) {
    BlockSignalDir(aut, block->side_a(), block->side_b(),
                   block->p()->r_main_sgn, block->p()->r_adv_sgn,
                   block->p()->r_in_adv_sgn,
                   rev_runaround ? *rev_runaround : runaround,
                   aut->ImportVariable(block->rev_route_out()));
  }
}

void MiddleSignal(Automata* aut, StandardMiddleSignal* piece, SignalVariable* main_sgn, SignalVariable* adv_sgn) {
  BlockSignalDir(aut, piece->side_b(), piece->side_a(), main_sgn, adv_sgn, nullptr, global_dispatch, aut->ImportVariable(piece->route_out()));
}

void XXOLDBlockSignal(Automata* aut, StandardBlock* block) {
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
    RgSignal(aut, aut->ImportVariable(block->rev_route_out()),
             aut->ImportVariable(block->p()->r_main_sgn));
  }
  if (block->p()->r_adv_sgn) {
    RgSignal(aut, aut->ImportVariable(block->rev_route_out()),
             aut->ImportVariable(block->p()->r_adv_sgn));
  }
  if (block->p()->r_in_adv_sgn) {
    RgSignal(aut, aut->ImportVariable(block->rev_route_out()),
             aut->ImportVariable(block->p()->r_in_adv_sgn));
  }
}

#if 0
DefAut(signalaut, brd, {
  BlockSignal(this, &Block_A461, runaround_olten);
  BlockSignal(this, &Block_B369, runaround_olten);
  ClearUsedVariables();
  BlockSignal(this, &Block_A441, runaround_lenzburg);
  BlockSignal(this, &Block_B349, runaround_lenzburg);
  ClearUsedVariables();
  BlockSignal(this, &Block_A431, runaround_spreitenbach);
  BlockSignal(this, &Block_B339, runaround_spreitenbach);
});

DefAut(signalaut1, brd, {
  BlockSignal(this, &Block_YYA13, runaround_yard, &runaround_rbl);
  BlockSignal(this, &Block_YYB22, runaround_rbl, &runaround_yard);
  ClearUsedVariables();
  BlockSignal(this, &Stub_XXB1.b_, global_dispatch);
  BlockSignal(this, &Stub_XXB2.b_, global_dispatch);
  ClearUsedVariables();
  BlockSignal(this, &Stub_XXB3.b_, global_dispatch);
  BlockSignal(this, &Block_XXB4, global_dispatch);
});

DefAut(signalaut2, brd, {
  BlockSignal(this, &Stub_YYA1.b_, runaround_yard);
  BlockSignal(this, &Stub_YYA2.b_, runaround_yard);
  ClearUsedVariables();
  BlockSignal(this, &Stub_YYA3.b_, runaround_yard);
  BlockSignal(this, &Block_YYA4, runaround_yard);
  ClearUsedVariables();
  BlockSignal(this, &Stub_YYA33.b_, runaround_rbl);
  BlockSignal(this, &Block_YYB42, runaround_rbl);
});

DefAut(signalaut3, brd, {
  ClearUsedVariables();
});
#endif


/*
DefAut(estopaut, brd, {
  Def().IfState(StInit).ActState(StBase);

  // Reports when a short is noticed by the CS. If th euser hit emergency off
  // on a throttle, this will not signal.
  auto* report = aut->ImportVariable(&estop_short);
  // Defines whether there is power on the track.
  auto* poweroff = aut->ImportVariable(&estop_nopwr);
  static const StateRef StRetry(NewUserState());
  static const StateRef StDead(NewUserState());

  // If we get a short report, we wait a bit first.
  Def()
      .IfState(StBase)
      .IfReg1(*report)
      .ActState(StWaiting)
      .ActTimer(4);

  // Then try to turn power on again.
  Def()
      .IfState(StWaiting)
      .IfTimerDone()
      .ActReg0(poweroff)
      .ActState(StRetry)
      .ActTimer(4);

  // If within another waiting period we get a second short report, we give up.
  Def().IfState(StRetry).IfReg1(*report).ActState(StDead);
  // If power is stable, we go to the base state.
  Def().IfState(StRetry).IfTimerDone().ActState(StBase);
  // When the user finally turns on power again, we should go back to base.
  Def().IfState(StDead).IfReg0(*poweroff).ActState(StBase);

  // Report is a single event in one direction -- we automatically reset the
  // internal bit to expect another report. Yes, this generates an extra event
  // on the bus but we don't care.
  Def().IfReg1(*report).ActReg0(report);
});
*/

uint64_t DccLongAddress(uint16_t addr) {
  if (addr < 128) {
    return 0x06010000C000ULL | addr;
  } else {
    return 0x060100000000ULL | addr;
  }
}

uint64_t DccShortAddress(uint16_t addr) {
  return 0x060100000000ULL | addr;
}

uint64_t MMAddress(uint16_t addr) {
  return 0x060300000000ULL | addr;
}


void IfAisleLoopFree(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW2.b.any_route()));
}
auto g_aisle_loop_free = NewCallback(&IfAisleLoopFree);


void IfWinLoopFree(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_YYW2.b.any_route()));
}
auto g_win_loop_free = NewCallback(&IfWinLoopFree);

/*
void IfZHEntry(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_ZHW1.b.any_route()));
}
auto g_zh_entry_free = NewCallback(&IfZHEntry);

void IfZHExit(Automata::Op* op) {
  IfNotPaused(op);
  IfNotShutdown(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_ZHW1.b.any_route()));
}
auto g_zh_exit_free = NewCallback(&IfZHExit);
*/


auto& Block_EntryToZH = Block_YYA2;

class LayoutSchedule : public TrainSchedule {
 public:
  LayoutSchedule(const string& name, uint64_t train_id, uint8_t default_speed)
      : TrainSchedule(name, &brd, train_id,
                      train_perm->Allocate(name, 24, 8),
                      train_tmp->Allocate(name, 48, 8), &stored_speed_),
        stored_speed_(&brd, "speed." + name,
                      BRACZ_SPEEDS | ((train_id & 0xff) << 8), default_speed) {}

 protected:
  /*
  void RunStubZH1(Automata* aut) {
    WithRouteLock l(this, &route_lock_ZH);
    AddDirectBlockTransition(Block_EntryToZH, Stub_ZHA1, &g_zh_entry_free);
    StopAndReverseAtStub(Stub_ZHA1);
    }*/

  void RunLoopCCW(Automata* aut) {
    AddDirectBlockTransition(Block_XXB1, Block_YYB1, &g_aisle_loop_free);
    AddDirectBlockTransition(Block_YYB1, Block_XXB1, &g_win_loop_free);
  }
 
  void RunLoopCW(Automata* aut) {
    AddDirectBlockTransition(Block_XXA2, Block_YYA2, &g_win_loop_free);
    AddDirectBlockTransition(Block_YYA2, Block_XXA2, &g_aisle_loop_free);
  }
 
#if 0
  void RunXtoY(Automata* aut) {
    AddDirectBlockTransition(Block_A461, Block_A441, &g_355_free);
    AddEagerBlockTransition(Block_A441, Block_A431);
    AddEagerBlockTransition(Block_A431, Block_YYA13);
  }
  void RunYtoX(Automata* aut) {
    AddEagerBlockTransition(Block_YYB22, Block_B339);
    AddEagerBlockTransition(Block_B339, Block_B349);
    AddDirectBlockTransition(Block_B349, Block_B369, &g_355_free);
  }

  void RunStubXX(Automata* aut) {
    WithRouteLock l(this, &route_lock_XX);
    AddBlockTransitionOnPermit(Block_EntryToXX, Stub_XXB3, &xx_tob3, &g_xx12_entry_free, false);
    SwitchTurnout(Turnout_XXW3.b.magnet(), false);
    SwitchTurnout(Turnout_XXW4.b.magnet(), false);

    StopAndReverseAtStub(Stub_XXB3);

    //AddBlockTransitionOnPermit(Block_EntryToXX, Stub_XXB4, &xx_tob4, &g_xx12_entry_free, false);
    //SwitchTurnout(Turnout_XXW3.b.magnet(), false);
    //SwitchTurnout(Turnout_XXW4.b.magnet(), true);

    //StopAndReverseAtStub(Stub_XXB4);

    AddBlockTransitionOnPermit(Block_EntryToXX, Stub_XXB2, &xx_tob2, &g_xx34_entry_free, false);
    SwitchTurnout(Turnout_XXW3.b.magnet(), true);
    SwitchTurnout(Turnout_XXW6.b.magnet(), false);

    StopAndReverseAtStub(Stub_XXB2);
    
    AddBlockTransitionOnPermit(Block_EntryToXX, Stub_XXB1, &xx_tob1, &g_xx34_entry_free, false);
    SwitchTurnout(Turnout_XXW3.b.magnet(), true);
    SwitchTurnout(Turnout_XXW6.b.magnet(), true);

    StopAndReverseAtStub(Stub_XXB1);

    AddBlockTransitionOnPermit(Stub_XXB3.b_.rev_signal, Block_ExitFromXX,
                               &xe_fromb3, &g_xx12_exit_free);
    SwitchTurnout(Turnout_XXW2.b.magnet(), true);
    //AddBlockTransitionOnPermit(Stub_XXB4.b_.rev_signal, Block_ExitFromXX,
    //                           &xe_fromb4, &g_xx12_exit_free);
    //SwitchTurnout(Turnout_XXW2.b.magnet(), true);
    AddBlockTransitionOnPermit(Stub_XXB2.b_.rev_signal, Block_ExitFromXX,
                               &xe_fromb2, &g_xx34_exit_free);
    SwitchTurnout(Turnout_XXW5.b.magnet(), false);
    AddBlockTransitionOnPermit(Stub_XXB1.b_.rev_signal, Block_ExitFromXX,
                               &xe_fromb1, &g_xx34_exit_free);
    SwitchTurnout(Turnout_XXW5.b.magnet(), false);
  }

  void RunLoopXX(Automata* aut) {
    {
      WithRouteLock l(this, &route_lock_XX);
      AddDirectBlockTransition(Block_EntryToXX, Block_ZZB2, &g_xx12_entry_free);
      SwitchTurnout(Turnout_XXW3.b.magnet(), false);
      SwitchTurnout(Turnout_XXW4.b.magnet(), true);
    }
    AddEagerBlockTransition(Block_ZZB2, Block_ZZA1);
    {
      WithRouteLock l(this, &route_lock_XX);
      AddBlockTransitionOnPermit(Block_ZZA1, Block_ExitFromXX,
                                 &xe_fromb4, &g_zz_exit_free);
      SwitchTurnout(Turnout_XXW2.b.magnet(), true);
    }
  }

  void RunStubYY(Automata* aut) {
    WithRouteLock l(this, &route_lock_YY);
    //AddBlockTransitionOnPermit(Block_EntryToYY, Stub_YYA4, &yy_toa4, &g_yy_entry_free);
    //SwitchTurnout(Turnout_YYW6.b.magnet(), true);
    
    //StopAndReverseAtStub(Stub_YYA4);

    AddBlockTransitionOnPermit(Block_EntryToYY, Stub_YYA3, &yy_toa3, &g_yy_entry_free);
    SwitchTurnout(Turnout_YYW6.b.magnet(), false);
    SwitchTurnout(Turnout_YYW7.b.magnet(), true);
    
    StopAndReverseAtStub(Stub_YYA3);

    AddBlockTransitionOnPermit(Block_EntryToYY, Stub_YYA2, &yy_toa2, &g_yy_entry_free);
    SwitchTurnout(Turnout_YYW6.b.magnet(), false);
    SwitchTurnout(Turnout_YYW7.b.magnet(), false);
    SwitchTurnout(Turnout_YYW8.b.magnet(), true);
    
    StopAndReverseAtStub(Stub_YYA2);

    AddBlockTransitionOnPermit(Block_EntryToYY, Stub_YYA1, &yy_toa1, &g_yy_entry_free);
    SwitchTurnout(Turnout_YYW6.b.magnet(), false);
    SwitchTurnout(Turnout_YYW7.b.magnet(), false);
    SwitchTurnout(Turnout_YYW8.b.magnet(), false);
    
    StopAndReverseAtStub(Stub_YYA1);

    /*AddBlockTransitionOnPermit(Stub_YYA4.b_.rev_signal,
                               Block_ExitFromYY,
                               &ye_froma4,
                               &g_yy_exit_free);
                               SwitchTurnout(Turnout_YYW4.b.magnet(), true);*/

    AddBlockTransitionOnPermit(Stub_YYA3.b_.rev_signal,
                               Block_ExitFromYY,
                               &ye_froma3,
                               &g_yy_exit_free);
    SwitchTurnout(Turnout_YYW4.b.magnet(), true);

    AddBlockTransitionOnPermit(Stub_YYA2.b_.rev_signal,
                               Block_ExitFromYY,
                               &ye_froma2,
                               &g_yy_exit_free);
    SwitchTurnout(Turnout_YYW4.b.magnet(), true);
    AddBlockTransitionOnPermit(Stub_YYA1.b_.rev_signal,
                               Block_ExitFromYY,
                               &ye_froma1,
                               &g_yy_exit_free);
    SwitchTurnout(Turnout_YYW4.b.magnet(), true);

    ClearAutomataVariables(aut);
  }

  void RunLoopYY(Automata* aut) {
    {
      WithRouteLock l(this, &route_lock_YY);
      AddDirectBlockTransition(Block_EntryToYY, Block_QQA2, &g_qq_entry_free);
      SwitchTurnout(Turnout_YYW6.b.magnet(), true);
    }
    AddEagerBlockTransition(Block_QQA2, Block_QQB3);
    {
      WithRouteLock l(this, &route_lock_YY);
      AddBlockTransitionOnPermit(Block_QQB3, Block_ExitFromYY, &ye_froma4,
                                 &g_qq_exit_free);
      SwitchTurnout(Turnout_YYW4.b.magnet(), true);
    }
  }
#endif
  
  void RunCycle(Automata* aut) {
    RunLoopCW(aut);
    RunLoopCCW(aut);
  }

  ByteImportVariable stored_speed_;
};



class CircleTrain : public LayoutSchedule {
 public:
  CircleTrain(const string& name, uint64_t train_id, uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    RunCycle(aut);
  }
};

#if 0

class IC2000Train : public LayoutSchedule {
 public:
  IC2000Train(const string& name, uint64_t train_id, uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    RunXtoY(aut);
    RunStubYY(aut);
    RunYtoX(aut);
    RunStubXX(aut);
  }
};


class IC2000TrainB : public LayoutSchedule {
 public:
  IC2000TrainB(const string& name, uint64_t train_id, uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    RunXtoY(aut);
    RunStubYY(aut);
    RunYtoX(aut);
    RunStubXX(aut);
  }
};

class FreightTrain : public LayoutSchedule {
 public:
  FreightTrain(const string& name, uint64_t train_id, uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    RunXtoY(aut);
    RunLoopYY(aut);
    RunYtoX(aut);
    RunLoopXX(aut);
  }
};


/*
CircleTrain train_rts("rts_railtraction", MMAddress(32), 10);
CircleTrain train_wle("wle_er20", MMAddress(27), 10);
CircleTrain train_re474("Re474", MMAddress(12), 30);
CircleTrain train_krokodil("Krokodil", MMAddress(68), 40);
CircleTrain train_rheingold("Rheingold", MMAddress(19), 35);
CircleTrain train_re10_10("Re_10_10", DccShortAddress(4), 35);
CircleTrain train_re460hag("Re460_HAG", DccShortAddress(26), 32);
IC2000Train train_re465("Re465", DccShortAddress(47), 17);
IC2000Train train_icn("ICN", DccShortAddress(50), 17);
IC2000Train train_ice("ICE", MMAddress(2), 16);
*/


/*
IC2000TrainB train_re460tsr("Re460-TSR", DccShortAddress(22), 35);
IC2000Train train_ice2("ICE2", DccShortAddress(40), 15);
IC2000Train train_re465("Re465", DccShortAddress(47), 25);
IC2000Train train_icn("ICN", DccShortAddress(50), 13);
IC2000Train train_bde44("BDe-4/4", DccShortAddress(38), 35);
FreightTrain train_re620("Re620", DccShortAddress(5), 45);
FreightTrain train_re420("Re420", DccShortAddress(4), 45);
FreightTrain train_rheingold("Rheingold", MMAddress(19), 35);
FreightTrain train_rts("rts_railtraction", DccShortAddress(32), 20);
FreightTrain train_wle("wle_er20", DccShortAddress(27), 20);
FreightTrain train_re474("Re474", MMAddress(12), 30);
FreightTrain train_re460hag("Re460_HAG", DccShortAddress(26), 32);
*/

#endif

CircleTrain train_re465("Re465", DccShortAddress(47), 25);
CircleTrain train_re460hag("Re460_HAG", DccShortAddress(26), 32);
CircleTrain train_rts("rts_railtraction", MMAddress(32), 10);
CircleTrain train_wle("wle_er20", MMAddress(27), 10);
CircleTrain train_re474("Re474", MMAddress(12), 30);
CircleTrain train_krokodil("Krokodil", MMAddress(68), 40);
CircleTrain train_rheingold("Rheingold", MMAddress(19), 35);
CircleTrain train_bde44("BDe-4/4", DccShortAddress(38), 35);
CircleTrain train_re620("Re620", DccShortAddress(5), 45);
CircleTrain train_re420("Re420", DccShortAddress(4), 45);


int main(int argc, char** argv) {
  automata::reset_routes = &reset_all_routes;

  FILE* f = fopen("automata.bin", "wb");
  assert(f);
  string output;
  brd.Render(&output);
  fwrite(output.data(), 1, output.size(), f);
  fclose(f);

  //f = fopen("bracz-layout2b-logic.cout", "wb");
  f = stdout;
  fprintf(f,
          "const char automata_code[] __attribute__((section(\"automata\"))) = "
          "{\n  ");
  int c = 0;
  for (char t : output) {
    fprintf(f, "0x%02x, ", (uint8_t)t);
    if (++c >= 12) {
      fprintf(f, "\n  ");
      c = 0;
    }
  }
  fprintf(f, "};\n");
  //fclose(f);

  f = fopen("variables.txt", "w");
  assert(f);
  map<uint64_t, string>& m(*automata::GetEventMap());
  for (const auto& it : m) {
    fprintf(f, "%016" PRIx64 ": %s\n", it.first, it.second.c_str());
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

  fprintf(stderr, "Allocator 1: %d entries remaining\n", l1.allocator()->remaining());
  fprintf(stderr, "Allocator 2: %d entries remaining\n", l2.allocator()->remaining());
  fprintf(stderr, "Allocator 3: %d entries remaining\n", l3.allocator()->remaining());
  fprintf(stderr, "Allocator %s: %d entries remaining\n",
          perm.allocator()->name().c_str(), perm.allocator()->remaining());

  return 0;
};
