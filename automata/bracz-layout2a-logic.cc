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

EventBasedVariable route_lock_HB(&brd, "route_lock_HB", BRACZ_LAYOUT | 0x0014,
                                 BRACZ_LAYOUT | 0x0015, 7, 30, 3);

/*EventBasedVariable route_lock_YY(&brd, "route_lock_YY", BRACZ_LAYOUT | 0x0016,
  BRACZ_LAYOUT | 0x0017, 7, 30, 2);*/


EventBasedVariable estop_short(&brd, "estop_short", BRACZ_LAYOUT | 0x0018,
                               BRACZ_LAYOUT | 0x0019, 7, 30, 1);

EventBasedVariable estop_nopwr(&brd, "estop", 0x010000000000FFFFULL,
                               0x010000000000FFFEULL, 7, 30, 0);

EventBasedVariable global_dispatch(&brd, "global_dispatch",
                                   BRACZ_LAYOUT | 0x001A, BRACZ_LAYOUT | 0x001B,
                                   7, 29, 7);

/*
constexpr auto SCENE_OFF_EVENT = BRACZ_LAYOUT | 0x0029;

EventBasedVariable scene_red_349(&brd, "scene_red_349",
                                   0x050101011808000A, SCENE_OFF_EVENT,
                                   7, 29, 0);

EventBasedVariable scene_green_349(&brd, "scene_green_349",
                                   0x050101011808000B, SCENE_OFF_EVENT,
                                   7, 28, 7);

EventBasedVariable scene_red_441(&brd, "scene_red_441",
                                   0x050101011808000D, SCENE_OFF_EVENT,
                                   7, 28, 6);

EventBasedVariable scene_green_441(&brd, "scene_green_441",
                                   0x050101011808000C, SCENE_OFF_EVENT,
                                   7, 28, 5);

EventBasedVariable scene_dark(&brd, "scene_dark",
                                   0x050101011808000E, SCENE_OFF_EVENT,
                                   7, 28, 4);
*/


// I2CBoard b5(0x25), b6(0x26); //, b7(0x27), b1(0x21), b2(0x22);
//NativeIO n9(0x29);
AccBoard ba(0x2a), bb(0x2b), bc(0x2e), bd(0x2d), b9(0x29), b8(0x28);

StateRef StUser1(10);
StateRef StUser2(11);

// 4-way bridge. Heads are from left to right.
I2CSignal signal_HBA1_main(&bb, 53, "HB.A1.main");
I2CSignal signal_HBA1_adv(&bb, 54, "HB.A1.adv");
I2CSignal signal_HBA2_main(&bb, 55, "HB.A2.main");
I2CSignal signal_HBA2_adv(&bb, 56, "HB.A2.adv");
I2CSignal signal_HBA3_main(&bb, 57, "HB.A3.main");
I2CSignal signal_HBA3_adv(&bb, 58, "HB.A3.adv");
I2CSignal signal_HBA4_main(&bb, 65, "HB.A4.main");
I2CSignal signal_HBA4_adv(&bb, 66, "HB.A4.adv");
// end four-way bridge.


// 2-way bridge with main signals on one side, advance on the other. Heads are
// from left to right (starting at main head).
I2CSignal signal_A139_main(&bb, 6, "A139.main");
I2CSignal signal_A139_adv(&bb, 7, "A139.adv");
I2CSignal signal_A239_main(&bb, 32, "A239.main");
I2CSignal signal_A239_adv(&bb, 33, "A239.adv");
I2CSignal signal_B239_adv(&bb, 75, "B239.adv");
I2CSignal signal_B139_adv(&bb, 74, "B139.adv");
// End bridge

// Double wired main-only head. Shorter wire:
I2CSignal signal_B241_main(&b8, 25, "B241.main");
// Longer wire
I2CSignal signal_B141_main(&b8, 24, "B141.main");

// This is one double-bridge with main+adv on one side and adv on the other
// side. Very short wire from the base with blob of hot glue.
I2CSignal signal_B249_adv(&bd, 19, "B249.adv");    // left mast
I2CSignal signal_B149_adv(&bd, 17, "B149.adv");    // middle mast

I2CSignal signal_A249_main(&bd, 14, "A249.main");  // middle mast
I2CSignal signal_A249_adv(&bd, 15, "A249.adv");    // middle mast

I2CSignal signal_A149_main(&bd, 12, "A149.main");  // left mast
I2CSignal signal_A149_adv(&bd, 13, "A149.adv");    // left mast
// end bridge

// Lone main-only head with a male-female plug.
I2CSignal signal_B251_main(&bd, 62, "B251.main");
// Lone main-only head with a male plug.
I2CSignal signal_B151_main(&bd, 63, "B151.main");


// Single mast M+A, thick wires and M+F plug with some angle between them.
I2CSignal signal_A279_main(&ba, 8, "A279.main");
I2CSignal signal_A279_adv(&ba, 9, "A279.adv");

// 2-way bridge. This has main signals on both sides. The first head is
// the one with the wires connected, then goes left-to-right and then other
// side left-to-right.
I2CSignal signal_B271_main(&ba, 36, "B271.main");
I2CSignal signal_B271_adv(&ba, 37, "B271.adv");
I2CSignal signal_B171_main(&ba, 42, "B171.main");
I2CSignal signal_B171_adv(&ba, 43, "B171.adv");
I2CSignal signal_A167_main(&ba, 40, "A167.main");
I2CSignal signal_A167_adv(&ba, 41, "A167.adv");
I2CSignal signal_A271_main(&ba, 38, "A271.main");
I2CSignal signal_A271_adv(&ba, 39, "A271.adv");
// End bridge.

// Single mast M+A about 10cm thick wires and single plug.
I2CSignal signal_A179_main(&ba, 26, "A179.main");
I2CSignal signal_A179_adv(&ba, 27, "A179.adv");

// Single mast M+A
I2CSignal signal_YEx_main(&ba, 22, "YEx.main");  // 157 (0x9D)
I2CSignal signal_YEx_adv(&ba, 23, "YEx.adv");

// Single mast M+A about 7cm thin wires, M+F plug.
I2CSignal signal_A259_main(&bc, 31, "A259.main");  // 159 (0x9F)
I2CSignal signal_A259_adv(&bc, 32, "A259.adv");


// Single mast M+A about 12 cm long thing ires with one taped with scotch
// tape. M plug.
I2CSignal signal_A159_main(&bc, 4, "A159.main");  // 141 (0x8D)
I2CSignal signal_A159_adv(&bc, 5, "A159.adv");

// Single mast M+A, M plug, 20cm long super thick wire.
I2CSignal signal_B161_main(&bc, 10, "B161.main");  // 131 0x83
I2CSignal signal_B161_adv(&bc, 11, "B161.adv");

// Single mast M+A, M plug, about 10cm super thick wire.
I2CSignal signal_B261_main(&bc, 34, "B261.main");
I2CSignal signal_B261_adv(&bc, 35, "B261.adv");

// Single mast M+A, M plug with blob of hot glue, about 7 cm thin wire.
I2CSignal signal_B231_main(&b9, 72, "B231.main");
I2CSignal signal_B231_adv(&b9, 73, "B231.adv");

// Single mast M+A, M plug with about 7 cm thin wire.
I2CSignal signal_B131_main(&b9, 44, "B131.main");  // 151 0x97
I2CSignal signal_B131_adv(&b9, 45, "B131.adv");


/*







// 4-way bridge (other). this has about 10cm wire from the base. Heads from left to right.
I2CSignal signal_YYB4_main(&ba, 69, "YY.B4.main");
I2CSignal signal_YYB4_adv(&ba, 70, "YY.B4.adv");
I2CSignal signal_YYB3_main(&ba, 67, "YY.B3.main");
I2CSignal signal_YYB3_adv(&ba, 68, "YY.B3.adv");
I2CSignal signal_YYB2_main(&ba, 51, "YY.B2.main");
I2CSignal signal_YYB2_adv(&ba, 52, "YY.B2.adv");
I2CSignal signal_YYB1_main(&ba, 49, "YY.B1.main");
I2CSignal signal_YYB1_adv(&ba, 50, "YY.B1.adv");
// end bridge


I2CSignal signal_YYA13_main(&ba, 22, "YY.A13.main");
I2CSignal signal_YYA13_adv(&ba, 23, "YY.A13.adv");
I2CSignal signal_YYA23_main(&ba, 44, "YY.A23.main");
I2CSignal signal_YYA23_adv(&ba, 45, "YY.A23.adv");
*/

/* More signals

PandaControlBoard panda_bridge;

LPC11C lpc11_back;


I2CSignal signal_A301_main(&bc, 72, "A301.main");
I2CSignal signal_A301_adv(&bc, 73, "A301.adv");
I2CSignal signal_A401_main(&bc, 34, "A401.main");
I2CSignal signal_A401_adv(&bc, 35, "A401.adv");

I2CSignal signal_A321_main(&bd, 36, "A321.main");  // 166 0xA6
I2CSignal signal_A321_adv(&bd, 37, "A321.adv");
I2CSignal signal_A421_main(&bd, 42, "A421.main");  // 134 (0x86)
I2CSignal signal_A421_adv(&bd, 43, "A421.adv");

I2CSignal signal_B321_main(&bd, 38, "B321.main");  // 172 0xAC
I2CSignal signal_B321_adv(&bd, 39, "B321.adv");
I2CSignal signal_B421_main(&bd, 40, "B421.main");  // 176 (0xB0)
I2CSignal signal_B421_adv(&bd, 41, "B421.adv");

I2CSignal signal_A347_main(&bd, 22, "A347.main");  // 157 (0x9D)
I2CSignal signal_A347_adv(&bd, 23, "A347.adv");
I2CSignal signal_B347_main(&bd, 8, "B347.main");  // 143 (0x8F)
I2CSignal signal_B347_adv(&bd, 9, "B347.adv");


I2CSignal signal_A360_main(&bb, 12, "A360.main");  // 142 (0x8E)
I2CSignal signal_A360_adv(&bb, 13, "A360.adv");
I2CSignal signal_A460_main(&bb, 14, "A460.main");  // 139 (0x8B)
I2CSignal signal_A460_adv(&bb, 15, "A460.adv");

I2CSignal signal_B460_main(&bb, 63, "B460.main");  // 191 (0xBF)
I2CSignal signal_B360_main(&bb, 62, "B360.main");  // 174 (0xAE)

I2CSignal signal_B360_adv(&bb, 17, "B360.adv");  // 138 (0x8A)
I2CSignal signal_B460_adv(&bb, 19, "B460.adv");  // 144 0x90

I2CSignal signal_A375_adv(&bb, 75, "A375.adv");  // 149 0x95
I2CSignal signal_A475_adv(&bb, 74, "A475.adv");  // 129 0x81

I2CSignal signal_B375_main(&bb, 32, "B375.main");  // 145 0x91
I2CSignal signal_B375_adv(&bb, 33, "B375.adv");
I2CSignal signal_B475_main(&bb, 6, "B475.main");  // 155 0x9B
I2CSignal signal_B475_adv(&bb, 7, "B475.adv");

I2CSignal signal_XXB1_main(&be, 25, "XX.B1.main");  // 147 (0x93)
I2CSignal signal_XXB3_main(&be, 24, "XX.B3.main");  // 140 (0x8C)

I2CSignal signal_YYC23_main(&ba, 26, "YY.C23.main");  //  158 (0x9E)
I2CSignal signal_YYC23_adv(&ba, 27, "YY.C23.adv");

// I2CSignal signal_WWB14_main(&bc, 34, "WW.B14.main"); // NOT REFLASHED
// I2CSignal signal_WWB14_adv(&bc, 35, "WW.B14.adv");

I2CSignal signal_WWB14_main(&bc, 69, "WW.B14.main");  // 173
I2CSignal signal_WWB14_adv(&bc, 70, "WW.B14.adv");
I2CSignal signal_WWB3_main(&bc, 67, "WW.B3.main");  // 167
I2CSignal signal_WWB3_adv(&bc, 68, "WW.B3.adv");
I2CSignal signal_WWB2_main(&bc, 51, "WW.B3.main");  // 171
I2CSignal signal_WWB2_adv(&bc, 52, "WW.B3.adv");
I2CSignal signal_WWB11_main(&bc, 49, "WW.B11.main");  // 164
I2CSignal signal_WWB11_adv(&bc, 50, "WW.B11.adv");

I2CSignal signal_A380_main(&bb, 53, "A380.main");  // 133
I2CSignal signal_A380_adv(&bb, 54, "A380.adv");
I2CSignal signal_A480_main(&bb, 55, "A480.main");  // 130
I2CSignal signal_A480_adv(&bb, 56, "A480.adv");
I2CSignal signal_ZZA2_main(&bb, 57, "ZZ.A2.main");  // 153
I2CSignal signal_ZZA2_adv(&bb, 58, "ZZ.A2.adv");
I2CSignal signal_ZZA3_main(&bb, 65, "ZZ.A3.main");  // 136
I2CSignal signal_ZZA3_adv(&bb, 66, "ZZ.A3.adv");

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

  // Turns on is_paused when the accessory bus turned off due to short etc.
  auto* lis_paused = ImportVariable(&is_paused);
  Def().IfReg1(signal_short).ActReg1(lis_paused);
  Def().IfReg1(signal_over).ActReg1(lis_paused);
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

  DefCopy(*rep, ImportVariable(&b8.LedBlueSw));
  DefCopy(*rep, ImportVariable(&b9.LedBlueSw));
  DefCopy(*rep, ImportVariable(&ba.LedBlueSw));
  DefCopy(*rep, ImportVariable(&bb.LedBlueSw));
  DefCopy(*rep, ImportVariable(&bc.LedBlueSw));
  DefCopy(*rep, ImportVariable(&bd.LedBlueSw));
  //DefCopy(*rep, ImportVariable(&be.LedBlueSw));
});

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
auto g_not_paused_condition = NewCallback(&IfNotPaused);
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
const AllocatorPtr& train_perm(perm.allocator());

/*
FlipFlopAutomata xx_flipflop(&brd, "xx_flipflop", logic, 32);
FlipFlopClient xx_tob1("to_B1", &xx_flipflop);
FlipFlopClient xx_tob2("to_B2", &xx_flipflop);
FlipFlopClient xx_tob3("to_B3", &xx_flipflop);
FlipFlopClient xx_tob4("to_B4", &xx_flipflop);
*/

FlipFlopAutomata hb_outflipflop(&brd, "hb_outflipflop", logic, 32);
FlipFlopClient hb_fromb1("from_B1", &hb_outflipflop);
FlipFlopClient hb_fromb2("from_B2", &hb_outflipflop);
FlipFlopClient hb_fromb3("from_B3", &hb_outflipflop);
FlipFlopClient hb_fromb4("from_B4", &hb_outflipflop);

/*
FlipFlopAutomata yy_flipflop(&brd, "yy_flipflop", logic, 32);
FlipFlopClient yy_toa1("to_A1", &yy_flipflop);
FlipFlopClient yy_toa2("to_A2", &yy_flipflop);
FlipFlopClient yy_toa3("to_A3", &yy_flipflop);
FlipFlopClient yy_toa4("to_A4", &yy_flipflop);

FlipFlopAutomata ye_flipflop(&brd, "ye_flipflop", logic, 32);
FlipFlopClient ye_froma1("from_A1", &ye_flipflop);
FlipFlopClient ye_froma2("from_A2", &ye_flipflop);
FlipFlopClient ye_froma3("from_A3", &ye_flipflop);
FlipFlopClient ye_froma4("from_A4", &ye_flipflop);
*/
AllocatorPtr train_tmp(logic->Allocate("train", 768));

MagnetCommandAutomata g_magnet_aut(&brd, logic);
MagnetPause magnet_pause(&g_magnet_aut, &power_acc);
MagnetButtonAutomata g_btn_aut(&brd);


MagnetDef Magnet_W230(&g_magnet_aut, "W230", &b9.ActBlueGrey,
                      &b9.ActBlueBrown, MovableTurnout::kThrown);
StandardMovableTurnout Turnout_W230(&brd,
                                    logic->Allocate("W230", 40),
                                    &Magnet_W230);
TurnoutWrap TW230(&Turnout_W230.b, kPointToThrown);

MagnetDef Magnet_BBW1(&g_magnet_aut, "BB.W1", &b9.ActGreenGreen,
                      &b9.ActGreenRed, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_BBW1(&brd,
                                    logic->Allocate("BB.W1", 40),
                                    &Magnet_BBW1);

MagnetDef Magnet_BBW2(&g_magnet_aut, "BB.W2", &b9.ActOraGreen,
                      &b9.ActOraRed, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_BBW2(&brd,
                                    logic->Allocate("BB.W2", 40),
                                    &Magnet_BBW2);


MagnetDef Magnet_HBW1(&g_magnet_aut, "HB.W1", &bb.ActBlueGrey,
                      &bb.ActBlueBrown, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_HBW1(&brd,
                                    logic->Allocate("HB.W1", 40),
                                    &Magnet_HBW1);
TurnoutWrap THBW1(&Turnout_HBW1.b, kPointToClosed);

MagnetDef Magnet_HBW2(&g_magnet_aut, "HB.W2", &b8.ActBlueGrey,
                      &b8.ActBlueBrown, MovableTurnout::kClosed);
StandardMovableDKW DKW_HBW2(&brd, logic->Allocate("HB.W2", 64),
                            &Magnet_HBW2);
DKWWrap THBW2Main(&DKW_HBW2.b, kB1toA1);
DKWWrap THBW2Cross(&DKW_HBW2.b, kA2toB2);

MagnetDef Magnet_HBW3(&g_magnet_aut, "HB.W3", &bb.ActBrownGrey,
                      &bb.ActBrownBrown, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_HBW3(&brd,
                                    logic->Allocate("HB.W3", 40),
                                    &Magnet_HBW3);
TurnoutWrap THBW3(&Turnout_HBW3.b, kPointToClosed);

// @todo: kDKWStateCross might be reversed.
MagnetDef Magnet_HBW4(&g_magnet_aut, "HB.W4", &bb.ActGreenGreen,
                      &bb.ActGreenRed, MovableTurnout::kClosed);
StandardMovableDKW DKW_HBW4(&brd, logic->Allocate("HB.W4", 64),
                            &Magnet_HBW4);
DKWWrap THBW4Main(&DKW_HBW4.b, kB1toA1);
DKWWrap THBW4Cross(&DKW_HBW4.b, kA2toB2);

MagnetDef Magnet_HBW5(&g_magnet_aut, "HB.W5", &bb.ActOraGreen,
                      &bb.ActOraRed, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_HBW5(&brd,
                                    logic->Allocate("HB.W5", 40),
                                    &Magnet_HBW5);
TurnoutWrap THBW5(&Turnout_HBW5.b, kPointToClosed);

MagnetDef Magnet_HBW6(&g_magnet_aut, "HB.W6", &b8.ActGreenGreen,
                      &b8.ActGreenRed, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_HBW6(&brd, logic->Allocate("HB.W6", 40),
                                    &Magnet_HBW6);
TurnoutWrap THBW6(&Turnout_HBW6.b, kClosedToPoint);

MagnetDef Magnet_HBW7(&g_magnet_aut, "HB.W7", &b8.ActOraGreen,
                      &b8.ActOraRed, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_HBW7(&brd, logic->Allocate("HB.W7", 40),
                                    &Magnet_HBW7);
TurnoutWrap THBW7(&Turnout_HBW7.b, kClosedToPoint);

CoupledMagnetDef Magnet_HBW8(&g_magnet_aut, "HB.W8", &Magnet_HBW7, false);
StandardMovableTurnout Turnout_HBW8(&brd, logic->Allocate("HB.W8", 40),
                                    &Magnet_HBW8);
TurnoutWrap THBW8(&Turnout_HBW8.b, kClosedToPoint);

PhysicalSignal A139(&bb.InOraGreen, nullptr, &signal_A139_main.signal,
                    &signal_A139_adv.signal, &signal_B131_main.signal,
                    &signal_B131_adv.signal, nullptr, &signal_B139_adv.signal);
// this one is not a real block, but a middle detector
PhysicalSignal A239(&bb.InOraRed, nullptr, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr);
PhysicalSignal A149(&bd.In6, nullptr, &signal_A149_main.signal,
                    &signal_A149_adv.signal, &signal_B141_main.signal, nullptr,
                    nullptr, &signal_B149_adv.signal);
PhysicalSignal A159(&bc.InBrownGrey, nullptr, &signal_A159_main.signal,
                    &signal_A159_adv.signal, &signal_B151_main.signal, nullptr,
                    nullptr, nullptr);
PhysicalSignal A167(&bc.InOraGreen, nullptr, &signal_A167_main.signal,
                    &signal_A167_adv.signal, &signal_B161_main.signal,
                    &signal_B161_adv.signal, nullptr, nullptr);
PhysicalSignal A179(&ba.InOraGreen, nullptr, &signal_A179_main.signal,
                    &signal_A179_adv.signal, &signal_B171_main.signal,
                    &signal_B171_adv.signal, nullptr, nullptr);
PhysicalSignal B271(&ba.In7, nullptr, &signal_B271_main.signal,
                    &signal_B271_adv.signal, &signal_A279_main.signal,
                    &signal_A279_adv.signal, nullptr, nullptr);
PhysicalSignal B261(&bc.InBrownBrown, nullptr, &signal_B261_main.signal,
                    &signal_B261_adv.signal, &signal_A271_main.signal,
                    &signal_A271_adv.signal, nullptr, nullptr);
PhysicalSignal B251(&bd.InBrownGrey, nullptr, &signal_B251_main.signal, nullptr,
                    &signal_A259_main.signal, &signal_A259_adv.signal, nullptr,
                    nullptr);
PhysicalSignal B241(&bb.InBrownBrown, nullptr, &signal_B241_main.signal,
                    nullptr, &signal_A249_main.signal, &signal_A249_adv.signal,
                    &signal_B249_adv.signal, nullptr);
PhysicalSignal B231(&b9.InBrownBrown, nullptr, &signal_B231_main.signal,
                    &signal_B231_adv.signal, &signal_A239_main.signal,
                    &signal_A239_adv.signal, &signal_B239_adv.signal, nullptr);

PhysicalSignal HBB1(&b9.InOraRed, nullptr, nullptr, nullptr,
                    &signal_HBA1_main.signal, &signal_HBA1_adv.signal, nullptr,
                    nullptr);
PhysicalSignal HBB2(&b9.InOraGreen, nullptr, nullptr, nullptr,
                    &signal_HBA2_main.signal, &signal_HBA2_adv.signal, nullptr,
                    nullptr);
PhysicalSignal HBB3(&b9.In6, nullptr, nullptr, nullptr,
                    &signal_HBA3_main.signal, &signal_HBA3_adv.signal, nullptr,
                    nullptr);
PhysicalSignal HBB4(&b9.In7, nullptr, nullptr, nullptr,
                    &signal_HBA4_main.signal, &signal_HBA4_adv.signal, nullptr,
                    nullptr);

PhysicalSignal BBB1(&b9.InOraRed, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr,
                    nullptr);
PhysicalSignal BBB2(&b9.InOraGreen, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr,
                    nullptr);
PhysicalSignal BBB3(&b9.In6, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr,
                    nullptr);

PhysicalSignal YEx(&ba.In6, nullptr, nullptr, nullptr,
                    &signal_YEx_main.signal, &signal_YEx_adv.signal, nullptr,
                    nullptr);


StandardMiddleDetector Det_239(&brd, A239.sensor_raw,
                               logic->Allocate("Det239", 32, 8));

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

PhysicalSignal YYA13(&ba.InOraGreen, nullptr, &signal_YYA13_main.signal,
                     &signal_YYA13_adv.signal, &signal_YYB12_main.signal,
                     nullptr, nullptr, nullptr);
PhysicalSignal YYB22(&bd.InGreenGreen, nullptr, &signal_YYB22_main.signal,
                     nullptr, &signal_YYA23_main.signal,
                     &signal_YYA23_adv.signal, nullptr, nullptr);

PhysicalSignal YYA33(&ba.In7, nullptr, nullptr, nullptr,
                     &signal_YYB32_main.signal, nullptr, nullptr, nullptr);

PhysicalSignal YYB42(&bd.InOraGreen, nullptr, &signal_YYB42_main.signal,
                     nullptr, nullptr, nullptr, nullptr, nullptr);

PhysicalSignal A431(&bd.InBrownBrown, nullptr, &signal_A431_main.signal,
                    &signal_A431_adv.signal, &signal_B439_main.signal,
                    &signal_B439_adv.signal, nullptr, &signal_B431_adv.signal);
PhysicalSignal B339(&bc.InOraGreen, nullptr, &signal_B339_main.signal,
                    &signal_B339_adv.signal, &signal_A331_main.signal,
                    &signal_A331_adv.signal, &signal_B331_adv.signal, nullptr);

PhysicalSignal A441(&bc.InOraRed, nullptr, &signal_A441_main.signal,
                    &signal_A441_adv.signal, &signal_B449_main.signal,
                    &signal_B449_adv.signal, nullptr, nullptr);
PhysicalSignal B349(&bc.InGreenGreen, nullptr, &signal_B349_main.signal,
                    &signal_B349_adv.signal, &signal_A341_main.signal,
                    &signal_A341_adv.signal, nullptr, nullptr);

PhysicalSignal A461(&bb.In4, nullptr, &signal_A461_main.signal,
                    &signal_A461_adv.signal, &signal_B469_main.signal,
                    &signal_B469_adv.signal, &signal_A469_adv.signal, nullptr);
PhysicalSignal B369(&bb.InBrownBrown, nullptr, &signal_B369_main.signal,
                    &signal_B369_adv.signal, &signal_A361_main.signal,
                    &signal_A361_adv.signal, nullptr, &signal_A369_adv.signal);

PhysicalSignal XXB4(&be.InBrownBrown, nullptr, nullptr, nullptr,
                    &signal_XXA4_main.signal, &signal_XXA4_adv.signal, nullptr,
                    nullptr);
PhysicalSignal XXB3(&be.InBrownGrey, nullptr, nullptr, nullptr,
                    &signal_XXA3_main.signal, &signal_XXA3_adv.signal, nullptr,
                    nullptr);
PhysicalSignal XXB2(&be.In7, nullptr, nullptr, nullptr,
                    &signal_XXA2_main.signal, &signal_XXA2_adv.signal, nullptr,
                    nullptr);
PhysicalSignal XXB1(&be.In6, nullptr, nullptr, nullptr,
                    &signal_XXA1_main.signal, &signal_XXA1_adv.signal, nullptr,
                    nullptr);

PhysicalSignal ZZB2(&be.InOraGreen, nullptr, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr);
PhysicalSignal ZZA1(&be.InOraRed, nullptr, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr);

*/

StubBlock Stub_HBB1(&brd, &HBB1, nullptr, logic, "HB.B1");
StubBlock Stub_HBB2(&brd, &HBB2, nullptr, logic, "HB.B2");
StubBlock Stub_HBB3(&brd, &HBB3, nullptr, logic, "HB.B3");
StubBlock Stub_HBB4(&brd, &HBB4, nullptr, logic, "HB.B4");

StubBlock Stub_BBB1(&brd, &BBB1, nullptr, logic, "BB.B1");
StubBlock Stub_BBB2(&brd, &BBB2, nullptr, logic, "BB.B2");
StubBlock Stub_BBB3(&brd, &BBB3, nullptr, logic, "BB.B3");

/*
StandardBlock Block_XXB4(&brd, &XXB4, logic, "XX.B4");
DefAut(xxb4_nostop, brd, {
    Def().ActReg1(aut->ImportVariable(Block_XXB4.signal_no_stop()));
    Def().ActReg1(aut->ImportVariable(Block_XXB4.rsignal_no_stop()));
  });

StandardFixedTurnout Turnout_ZZW1(&brd, logic->Allocate("ZZ.W1", 40),
                                  FixedTurnout::TURNOUT_THROWN);
StandardBlock Block_ZZB2(&brd, &ZZB2, logic, "ZZ.B2");
StandardBlock Block_ZZA1(&brd, &ZZA1, logic, "ZZ.A1");
TurnoutWrap TZZW1(&Turnout_ZZW1.b, kPointToThrown);
*/

StandardBlock Block_A139(&brd, &A139, logic, "A139");
StandardBlock Block_A149(&brd, &A149, logic, "A149");
StandardBlock Block_A159(&brd, &A159, logic, "A159");
StandardBlock Block_A167(&brd, &A167, logic, "A167");
StandardBlock Block_A179(&brd, &A179, logic, "A179");
StandardBlock Block_B271(&brd, &B271, logic, "B271");
StandardBlock Block_B261(&brd, &B261, logic, "B261");
StandardBlock Block_B251(&brd, &B251, logic, "B251");
StandardBlock Block_B241(&brd, &B241, logic, "B241");
StandardBlock Block_B231(&brd, &B231, logic, "B231");


MagnetDef Magnet_W250(&g_magnet_aut, "W250", &bd.ActOraGreen, &bd.ActOraRed,
                      MovableTurnout::kClosed);
StandardMovableTurnout Turnout_W250(&brd, logic->Allocate("W250", 40),
                                    &Magnet_W250);
TurnoutWrap TW250(&Turnout_W250.b, kClosedToPoint);

CoupledMagnetDef Magnet_W150(&g_magnet_aut, "W150", &Magnet_W250, false);
StandardMovableTurnout Turnout_W150(&brd, logic->Allocate("W150", 40), &Magnet_W150);
TurnoutWrap TW150(&Turnout_W150.b, kClosedToPoint);


MagnetDef Magnet_W159(&g_magnet_aut, "W159", &bc.ActGreenGreen, &bc.ActGreenRed,
                      MovableTurnout::kClosed);
StandardMovableTurnout Turnout_W159(&brd, logic->Allocate("W159", 40),
                                    &Magnet_W159);
TurnoutWrap TW159(&Turnout_W159.b, kPointToClosed);

CoupledMagnetDef Magnet_W161(&g_magnet_aut, "W161", &Magnet_W159, false);
StandardMovableTurnout Turnout_W161(&brd, logic->Allocate("W161", 40), &Magnet_W161);
TurnoutWrap TW161(&Turnout_W161.b, kClosedToPoint);

CoupledMagnetDef Magnet_DKW260(&g_magnet_aut, "DKW260", &Magnet_W159, true);
StandardMovableDKW DKW_260(&brd, logic->Allocate("DKW260", 64),
                            &Magnet_DKW260);
DKWWrap TDKW260Main(&DKW_260.b, kA2toB1);


MagnetDef Magnet_W270(&g_magnet_aut, "W270", &ba.ActOraGreen, &ba.ActOraRed,
                      MovableTurnout::kClosed);
StandardMovableTurnout Turnout_W270(&brd, logic->Allocate("W270", 40),
                                    &Magnet_W270);
TurnoutWrap TW270(&Turnout_W270.b, kPointToClosed);

CoupledMagnetDef Magnet_W170(&g_magnet_aut, "W170", &Magnet_W270, false);
StandardMovableTurnout Turnout_W170(&brd, logic->Allocate("W170", 40), &Magnet_W170);
TurnoutWrap TW170(&Turnout_W170.b, kPointToClosed);


MagnetDef Magnet_W180(&g_magnet_aut, "W180", &ba.ActBlueGrey, &ba.ActBlueBrown,
                      MovableTurnout::kThrown);
StandardMovableTurnout Turnout_W180(&brd, logic->Allocate("W180", 40),
                                    &Magnet_W180);
TurnoutWrap TW180(&Turnout_W180.b, kPointToThrown);

MagnetDef Magnet_W280(&g_magnet_aut, "W280", &ba.ActBrownGrey,
                      &ba.ActBrownBrown, MovableTurnout::kThrown);
StandardMovableTurnout Turnout_W280(&brd, logic->Allocate("W280", 40),
                                    &Magnet_W280);
TurnoutWrap TW280(&Turnout_W280.b, kThrownToPoint);


MagnetDef Magnet_YDW1(&g_magnet_aut, "YD.W1", &ba.ActGreenGreen,
                      &b9.ActGreenRed, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_YDW1(&brd,
                                    logic->Allocate("YD.W1", 40),
                                    &Magnet_YDW1);


StubBlock Stub_Yex(&brd, &YEx, nullptr, logic, "YD.Ex");


/*
MagnetDef Magnet_W360(&g_magnet_aut, "W360", &bb.ActBlueGrey, &bb.ActBlueBrown,
                      MovableTurnout::kThrown);
StandardMovableTurnout Turnout_W360(&brd, logic->Allocate("W360", 40),
                                    &Magnet_W360);
TurnoutWrap TW360(&Turnout_W360.b, kClosedToPoint);
MagnetButton BtnW360(&g_btn_aut, logic2, &Magnet_W360, bb.InGreenYellow);

MagnetDef Magnet_W349(&g_magnet_aut, "W349", &bc.ActOraGreen,
                      &bc.ActOraRed, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_W349(&brd, logic->Allocate("W349", 40), &Magnet_W349);
TurnoutWrap TW349(&Turnout_W349.b, kPointToClosed);
MagnetButton BtnW349(&g_btn_aut, logic2, &Magnet_W349, bc.InGreenYellow);

StandardMiddleLongTrack Mdl450(&brd, logic->Allocate("Mdl450", 24));

StandardBlock Block_A441(&brd, &A441, logic, "A441");
StandardBlock Block_A431(&brd, &A431, logic, "A431");

MagnetDef Magnet_W440(&g_magnet_aut, "W440", &bc.ActBlueGrey, &bc.ActBlueBrown, MovableTurnout::kThrown);
MagnetButton BtnW440(&g_btn_aut, logic2, &Magnet_W440, bc.InBrownBrown);
StandardMovableTurnout Turnout_W440(&brd, logic->Allocate("W440", 40), &Magnet_W440);
TurnoutWrap TW440(&Turnout_W440.b, kThrownToPoint);

CoupledMagnetDef Magnet_W340(&g_magnet_aut, "W340", &Magnet_W440, true);
StandardMovableTurnout Turnout_W340(&brd, logic->Allocate("W340", 40), &Magnet_W340);
TurnoutWrap TW340(&Turnout_W340.b, kClosedToPoint);

StandardBlock Block_B349(&brd, &B349, logic, "B349");
StandardBlock Block_B339(&brd, &B339, logic, "B339");

MagnetDef Magnet_YYW2(&g_magnet_aut, "YY.W2", &bd.ActOraGreen,
                      &bd.ActOraRed, MovableTurnout::kClosed);
MagnetButton BtnYYW2(&g_btn_aut, logic2, &Magnet_YYW2, bd.InBrownGrey);
StandardMovableTurnout Turnout_YYW2(&brd, logic->Allocate("YY.W2", 40), &Magnet_YYW2);
TurnoutWrap TYYW2(&Turnout_YYW2.b, kPointToClosed);

CoupledMagnetDef Magnet_YYW1(&g_magnet_aut, "YY.W1", &Magnet_YYW2, true);
StandardMovableTurnout Turnout_YYW1(&brd, logic->Allocate("YY.W1", 40), &Magnet_YYW1);
TurnoutWrap TYYW1(&Turnout_YYW1.b, kPointToThrown);

StandardBlock Block_YYA13(&brd, &YYA13, logic, "YY.A13");
StandardBlock Block_YYB22(&brd, &YYB22, logic, "YY.B22");

StandardFixedTurnout Turnout_YYW3(&brd, logic->Allocate("YY.W3", 40),
                                  FixedTurnout::TURNOUT_CLOSED);
TurnoutWrap TYYW3(&Turnout_YYW3.b, kClosedToPoint);

StandardFixedTurnout Turnout_YYW9(&brd, logic->Allocate("YY.W9", 40),
                                  FixedTurnout::TURNOUT_CLOSED);
TurnoutWrap TYYW9(&Turnout_YYW9.b, kThrownToPoint);

MagnetDef Magnet_YYW4(&g_magnet_aut, "YY.W4", &ba.ActBlueGrey,
                      &ba.ActBlueBrown, MovableTurnout::kThrown);
StandardMovableTurnout Turnout_YYW4(&brd, logic->Allocate("YY.W4", 40), &Magnet_YYW4);
TurnoutWrap TYYW4(&Turnout_YYW4.b, kClosedToPoint);

StandardFixedTurnout Turnout_YYW5(&brd, logic->Allocate("YY.W5", 40),
                                  FixedTurnout::TURNOUT_THROWN);
TurnoutWrap TYYW5(&Turnout_YYW5.b, kThrownToPoint);

MagnetDef Magnet_YYW6(&g_magnet_aut, "YY.W6", &ba.ActOraGreen,
                      &ba.ActOraRed, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_YYW6(&brd, logic->Allocate("YY.W6", 40), &Magnet_YYW6);
TurnoutWrap TYYW6(&Turnout_YYW6.b, kPointToClosed);

MagnetDef Magnet_YYW7(&g_magnet_aut, "YY.W7", &ba.ActGreenGreen,
                      &ba.ActGreenRed, MovableTurnout::kThrown);
StandardMovableTurnout Turnout_YYW7(&brd, logic->Allocate("YY.W7", 40), &Magnet_YYW7);
TurnoutWrap TYYW7(&Turnout_YYW7.b, kPointToClosed);

MagnetDef Magnet_YYW8(&g_magnet_aut, "YY.W8", &ba.ActBrownGrey,
                      &ba.ActBrownBrown, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_YYW8(&brd, logic->Allocate("YY.W8", 40), &Magnet_YYW8);
TurnoutWrap TYYW8(&Turnout_YYW8.b, kPointToClosed);


StubBlock Stub_YYA1(&brd, &YYA1, nullptr, logic, "YY.A1");
StubBlock Stub_YYA2(&brd, &YYA2, nullptr, logic, "YY.A2");
StubBlock Stub_YYA3(&brd, &YYA3, nullptr, logic, "YY.A3");
StandardBlock Block_YYA4(&brd, &YYA4, logic, "YY.A4");
DefAut(yya4_nostop, brd, {
    Def().ActReg1(aut->ImportVariable(Block_YYA4.signal_no_stop()));
    Def().ActReg1(aut->ImportVariable(Block_YYA4.rsignal_no_stop()));
  });


StubBlock Stub_YYA33(&brd, &YYA33, nullptr, logic, "YY.A33");
StandardBlock Block_YYB42(&brd, &YYB42, logic, "YY.B42");

StandardFixedTurnout Turnout_QQW1(&brd, logic->Allocate("QQ.W1", 40), FixedTurnout::TURNOUT_CLOSED);
TurnoutWrap TQQW1(&Turnout_QQW1.b, kPointToClosed);
StandardFixedTurnout Turnout_QQW2(&brd, logic->Allocate("QQ.W2", 40), FixedTurnout::TURNOUT_THROWN);
TurnoutWrap TQQW2(&Turnout_QQW2.b, kPointToThrown);

StubBlock Stub_QQA1(&brd, &QQA1, nullptr, logic, "QQ.A1");
StandardBlock Block_QQA2(&brd, &QQA2, logic, "QQ.A2");
StandardBlock Block_QQB3(&brd, &QQB3, logic, "QQ.B3");
*/

bool ignored1 = BindSequence(
    Turnout_HBW1.b.side_thrown(),
    {  &THBW2Cross, &THBW6 },
    Stub_HBB1.entry());

bool ignored1a = BindSequence(
    Turnout_HBW3.b.side_thrown(),
    {  &THBW4Cross, &THBW5},
    Stub_HBB3.entry());

bool ignored2 = BindSequence(  //
    Block_B241.side_b(),
    {&THBW1,      &THBW3,     &Det_239,    &THBW8,                //
     &Block_B231, &TW230,     &Block_A139,                        //
     &THBW7,      &THBW4Main, &THBW2Main,                         //
     &Block_A149, &TW150,     &Block_A159, &TW159,       &TW161,  //
     &Block_A167, &TW170,     &Block_A179, &TW180,       &TW280,  //
     &Block_B271, &TW270,     &Block_B261, &TDKW260Main, &Block_B251, &TW250},
    Block_B241.side_a());

bool ignored3 = BindPairs({
    //
    {Stub_HBB4.entry(), Turnout_HBW5.b.side_thrown()},
    {Stub_HBB2.entry(), Turnout_HBW6.b.side_thrown()},
    {Turnout_W250.b.side_thrown(), Turnout_W150.b.side_thrown()},
    {Turnout_W159.b.side_thrown(), DKW_260.b.point_b2()},
    {Turnout_W161.b.side_thrown(), DKW_260.b.point_a1()},
    {Turnout_W270.b.side_thrown(), Turnout_W170.b.side_thrown()},
    {Turnout_W280.b.side_closed(), Turnout_YDW1.b.side_thrown()},
    {Turnout_YDW1.b.side_closed(), Turnout_W180.b.side_closed()},
    {Turnout_YDW1.b.side_points(), Stub_Yex.entry()},
    {Turnout_W230.b.side_closed(), Turnout_BBW1.b.side_points()},
    {Turnout_BBW1.b.side_closed(), Turnout_BBW2.b.side_points()},
    {Turnout_BBW2.b.side_closed(), Stub_BBB1.entry()},
    {Turnout_BBW2.b.side_thrown(), Stub_BBB2.entry()},
    {Turnout_BBW1.b.side_thrown(), Stub_BBB3.entry()},
    {Turnout_HBW7.b.side_thrown(), Turnout_HBW8.b.side_thrown()}

    /*        
    {Stub_XXB1.entry(), Turnout_XXW6.b.side_thrown()},
    {Stub_XXB3.entry(), Turnout_XXW4.b.side_closed()},
    {Turnout_XXW5.b.side_thrown(), Turnout_XXW3.b.side_thrown()},
    {Turnout_XXW1.b.side_closed(), Turnout_XXW2.b.side_thrown()},
    {Turnout_YYW7.b.side_thrown(), Stub_YYA3.entry()},
    {Turnout_YYW8.b.side_thrown(), Stub_YYA2.entry()},
    {Turnout_YYW1.b.side_closed(), Turnout_YYW2.b.side_thrown()},
    {Turnout_YYW9.b.side_closed(), Stub_YYA33.entry()},
    {Turnout_QQW2.b.side_closed(), Stub_QQA1.entry()},
    {Turnout_W340.b.side_thrown(), Turnout_W440.b.side_closed()} 
    */
 //
});

/*bool ignored2 = BindSequence(  //
    Stub_XXB2.entry(),         //
    {&TXXW6, &TXXW5, &TXXW1, &Block_A461, &TW360, &Mdl450, &TW349, &Block_A441,
     &TW440, &Block_A431, &TYYW1, &Block_YYA13, &TYYW4, &TYYW5, &TYYW6, &TYYW7,
     &TYYW8},
    Stub_YYA1.entry());

bool ignored4 = BindSequence(  //
    Turnout_YYW4.b.side_thrown(),
    {&Block_YYB22, &TYYW3, &TYYW2, &Block_B339, &TW340, &Block_B349},
    Turnout_W349.b.side_thrown());

bool ignored3 = BindSequence(Turnout_W360.b.side_thrown(),
                             {&Block_B369, &TXXW2, &TXXW3, &TXXW4, &Block_XXB4,
                              &TZZW1, &Block_ZZB2, &Block_ZZA1},
                             Turnout_ZZW1.b.side_closed());

bool ignored5 = BindSequence(  //
    Turnout_YYW5.b.side_closed(),
    {&Block_YYB42, &TYYW9},
    Turnout_YYW3.b.side_thrown());

bool ignored6 = BindSequence( //
    Turnout_YYW6.b.side_thrown(), //
    { &Block_YYA4, &TQQW1, &TQQW2, &Block_QQA2, &Block_QQB3 }, //
    Turnout_QQW1.b.side_thrown());


auto& Block_EntryToXX = Block_B369;
auto& Block_ExitFromXX = Block_A461;
auto& Block_EntryToYY = Block_YYA13;
auto& Block_ExitFromYY = Block_YYB22;

*/



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

void RgSignal(Automata* aut, const Automata::LocalVariable& route_set,
              Automata::LocalVariable* signal) {
  Def().IfReg0(route_set).ActSetValue(signal, 1, A_STOP);
  Def().IfReg1(route_set).ActSetValue(signal, 1, A_90);
}

void RedSignal(Automata* aut, Automata::LocalVariable* signal) {
  Def().ActSetValue(signal, 1, A_STOP);
}

/// Computes the signal aspects for a standard block's signals in one direction
/// (i.e. east or west). That is three signals: the exit mast main and advance,
/// and the entry mast advance signal.
///
/// @param aut parent automata, usually just 'aut'.
/// @param side_front The interface where the train will be going (i.e. just
/// beyond the exit main signal).
/// @param side_back The interface where the train entered this block (where the
/// incoming advance signal mast is).
/// @param main_sgn Pointer to the main signal head at the exit.
/// @param adv_sgn Pointer to the advance signal head at the exit.
/// @param in_adv_sgn Pointer to the advance signal head in the entry.
/// @param runaround Variable that forces all signals to pass-at-sight. If no
/// local value for this, set it to 'global_dispatch'.
/// @param green Variable that tells whether a train can pass the exit signal.
///
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

DefAut(signalaut, brd, {
  BlockSignal(this, &Stub_HBB1.b_, global_dispatch);
  BlockSignal(this, &Stub_HBB2.b_, global_dispatch);
  ClearUsedVariables();
  BlockSignal(this, &Stub_HBB3.b_, global_dispatch);
  BlockSignal(this, &Stub_HBB4.b_, global_dispatch);
  ClearUsedVariables();
  BlockSignal(this, &Block_A139, global_dispatch);
  BlockSignal(this, &Block_B231, global_dispatch);
});

DefAut(signalaut1, brd, {
  BlockSignal(this, &Block_A149, global_dispatch);
  BlockSignal(this, &Block_B241, global_dispatch);
  ClearUsedVariables();
  BlockSignal(this, &Block_A159, global_dispatch);
  BlockSignal(this, &Block_B251, global_dispatch);
  ClearUsedVariables();
  BlockSignal(this, &Block_A179, global_dispatch);
  BlockSignal(this, &Block_B271, global_dispatch);
  //BlockSignal(this, &Stub_XXB3.b_, global_dispatch);
  //BlockSignal(this, &Block_XXB4, global_dispatch);
});

DefAut(signalaut2, brd, {
  BlockSignal(this, &Block_A167, global_dispatch);
  BlockSignal(this, &Block_B261, global_dispatch);
  ClearUsedVariables();
  //BlockSignal(this, &Stub_YYA3.b_, runaround_yard);
  //BlockSignal(this, &Block_YYA4, runaround_yard);
  ClearUsedVariables();
  //BlockSignal(this, &Stub_YYA33.b_, runaround_rbl);
  //BlockSignal(this, &Block_YYB42, runaround_rbl);
});
/*
DefAut(signalaut3, brd, {
  ClearUsedVariables();
});
*/

/* 

// This code is guessing the next signal aspect of a signal and sets the scene
// for the light to point at.

EventBasedVariable listen_B349_req_green(
    &brd, "listen_B349_req_green", Block_B349.request_green()->event_on(),
    BRACZ_LAYOUT | 0x002A, 7, 28, 3);

EventBasedVariable listen_A441_req_green(
    &brd, "listen_A441_req_green", Block_A441.request_green()->event_on(),
    BRACZ_LAYOUT | 0x002B, 7, 28, 2);

std::unique_ptr<GlobalVariable> b349_green_guess{
    logic->Allocate("b349-green-guess")};

DefAut(guessaut, brd, {
  static const StateRef StGuessGreen(NewUserState());
  static const StateRef StGuessRed(NewUserState());

  auto* guess = ImportVariable(b349_green_guess.get());
  Def().ActState(StGuessGreen);
  Def().IfReg1(ImportVariable(is_paused)).ActState(StGuessRed);
  Def().IfReg1(ImportVariable(reset_all_routes)).ActState(StGuessRed);
  Def().IfReg1(ImportVariable(Block_B369.route_in())).ActState(StGuessRed);
  Def()
      .IfReg1(aut->ImportVariable(*Turnout_W360.b.any_route()))
      .ActState(StGuessRed);
  Def().IfReg1(aut->ImportVariable(Block_B369.detector())).ActState(StGuessRed);
  Def().IfState(StGuessRed).ActReg0(guess);
  Def().IfState(StGuessGreen).ActReg1(guess);
});

DefAut(lightaut, brd, {
    static const StateRef StB349Green(NewUserState());
    static const StateRef StB349Red(NewUserState());
    static const StateRef StA441Green(NewUserState());
    static const StateRef StA441Red(NewUserState());
    static const StateRef StDark(NewUserState());
    static std::unique_ptr<GlobalVariable> b339_det_shadow{logic->Allocate("b339-det-shadow")};

    auto* scene_B349_red = aut->ImportVariable(&scene_red_349);
    auto* scene_B349_green = aut->ImportVariable(&scene_green_349);
    auto* B349_req_green = aut->ImportVariable(&listen_B349_req_green);
    auto* scene_A441_red = aut->ImportVariable(&scene_red_441);
    auto* scene_A441_green = aut->ImportVariable(&scene_green_441);
    auto* A441_req_green = aut->ImportVariable(&listen_A441_req_green);

    auto* sc_dark = aut->ImportVariable(&scene_dark);

    Def()
        .IfReg1(*B349_req_green)
        .ActState(StB349Green)
        .ActTimer(5)
        .ActReg1(scene_B349_green)
        .ActReg0(B349_req_green)
        .ActReg0(scene_B349_green);
    Def()
        .IfState(StB349Green)
        .IfTimerDone()
        .IfReg0(aut->ImportVariable(Block_B349.route_out()))
        .ActReg1(scene_B349_red)
        .ActReg0(scene_B349_red)
        .ActState(StB349Red)
        .ActTimer(10);
    Def()
        .IfState(StB349Red)
        .IfTimerDone()
        .ActReg1(sc_dark)
        .ActReg0(sc_dark)
        .ActState(StDark);
    Def()
        .IfReg1(*A441_req_green)
        .ActReg1(scene_A441_green)
        .ActReg0(scene_A441_green)
        .ActReg0(A441_req_green)
        .ActState(StA441Green)
        .ActTimer(5);

    Def()
        .IfState(StA441Green)
        .IfTimerDone()
        .IfReg0(aut->ImportVariable(Block_A441.route_out()))
        .ActReg1(scene_A441_red)
        .ActReg0(scene_A441_red)
        .ActState(StA441Red)
        .ActTimer(10);
    Def()
        .IfState(StA441Red)
        .IfTimerDone()
        .ActReg1(sc_dark)
        .ActReg0(sc_dark)
        .ActState(StDark);

    auto prev_block_detector = aut->ImportVariable(Block_B339.detector());
    auto* prev_block_det_shadow = aut->ImportVariable(b339_det_shadow.get());
    auto guess_green = aut->ImportVariable(*b349_green_guess);
    Def()
        .IfState(StInit)
        .IfReg0(prev_block_detector)
        .ActReg0(prev_block_det_shadow);
    Def()
        .IfReg1(prev_block_detector)
        .ActReg1(prev_block_det_shadow);

    // If there is an incoming train to B349
    Def().IfReg1(*prev_block_det_shadow)
        .IfReg0(prev_block_detector)
        .IfReg0(guess_green)
        .ActReg0(prev_block_det_shadow)
        .ActReg1(scene_B349_red)
        .ActReg0(scene_B349_red)
        .ActState(StB349Red)
        .ActTimer(20);
    Def().IfReg1(*prev_block_det_shadow)
        .IfReg0(prev_block_detector)
        .IfReg1(guess_green)
        .ActReg0(prev_block_det_shadow)
        .ActReg1(scene_B349_green)
        .ActReg0(scene_B349_green)
        .ActState(StB349Green)
        .ActTimer(20);

    Def()
        .IfState(StInit)
        .ActState(StDark)
        .ActReg1(sc_dark)
        .ActReg0(sc_dark);

    ClearUsedVariables();
});
*/

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



/*
void IfDKW209Free(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*DKW_W209.b.any_route()));
}

auto g_dkw209_free = NewCallback(&IfDKW209Free);


void IfXXW2Free(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*DKW_XXW2.b.any_route()));
}

auto g_xxw2_free = NewCallback(&IfXXW2Free);
*/


/*
void IfXX12EntryFree(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW2.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW4.b.any_route()));
}
auto g_xx12_entry_free = NewCallback(&IfXX12EntryFree);

void IfXX34EntryFree(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW2.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW6.b.any_route()));
}
auto g_xx34_entry_free = NewCallback(&IfXX34EntryFree);

void IfXX12ExitFree(Automata::Op* op) {
  IfNotPaused(op);
  IfNotShutdown(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW1.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW2.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW4.b.any_route()));
}
auto g_xx12_exit_free = NewCallback(&IfXX12ExitFree);

void IfZZExitFree(Automata::Op* op) {
  IfNotPaused(op);
  IfNotShutdown(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW1.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW2.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW4.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*Turnout_ZZW1.b.any_route()));
}
auto g_zz_exit_free = NewCallback(&IfZZExitFree);

void IfXX34ExitFree(Automata::Op* op) {
  IfNotPaused(op);
  IfNotShutdown(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW1.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*Turnout_XXW6.b.any_route()));
}
auto g_xx34_exit_free = NewCallback(&IfXX34ExitFree);



void IfYYEntryFree(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_YYW6.b.any_route()));
}
auto g_yy_entry_free = NewCallback(&IfYYEntryFree);

void IfQQEntryFree(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_YYW6.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*Turnout_QQW1.b.any_route()));
}
auto g_qq_entry_free = NewCallback(&IfQQEntryFree);

void IfYYExitFree(Automata::Op* op) {
  IfNotPaused(op);
  IfNotShutdown(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_YYW6.b.any_route()));
}
auto g_yy_exit_free = NewCallback(&IfYYExitFree);

void IfQQExitFree(Automata::Op* op) {
  IfNotPaused(op);
  IfNotShutdown(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_YYW6.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*Turnout_QQW1.b.any_route()));
}
auto g_qq_exit_free = NewCallback(&IfQQExitFree);


void If355Free(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_W360.b.any_route()));
}
auto g_355_free = NewCallback(&If355Free);
*/

void IfHBBothDKWFreeIn(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*DKW_HBW4.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*DKW_HBW2.b.any_route()));
}
auto g_hb_both_dkw_free_in = NewCallback(&IfHBBothDKWFreeIn);

void IfHBBothDKWFreeOut(Automata::Op* op) {
  IfNotPaused(op);
  IfNotShutdown(op);
  op->IfReg0(op->parent()->ImportVariable(*DKW_HBW4.b.any_route()));
  op->IfReg0(op->parent()->ImportVariable(*DKW_HBW2.b.any_route()));
}
auto g_hb_both_dkw_free_out = NewCallback(&IfHBBothDKWFreeOut);

void IfHBDKW2FreeIn(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*DKW_HBW2.b.any_route()));
}
auto g_hb_dkw2_free_in = NewCallback(&IfHBDKW2FreeIn);

void IfHBDKW2FreeOut(Automata::Op* op) {
  IfNotPaused(op);
  IfNotShutdown(op);
  op->IfReg0(op->parent()->ImportVariable(*DKW_HBW2.b.any_route()));
}
auto g_hb_dkw2_free_out = NewCallback(&IfHBDKW2FreeOut);

void IfHBDKW4FreeIn(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*DKW_HBW4.b.any_route()));
}
auto g_hb_dkw4_free_in = NewCallback(&IfHBDKW4FreeIn);



class LayoutSchedule : public TrainSchedule {
 public:
  LayoutSchedule(const string& name, uint64_t train_id, uint8_t default_speed)
      : TrainSchedule(name, &brd, train_id,
                      train_perm->Allocate(name, 24, 8),
                      train_tmp->Allocate(name, 48, 8), &stored_speed_),
        stored_speed_(&brd, "speed." + name,
                      BRACZ_SPEEDS | ((train_id & 0xff) << 8), default_speed) {}

 protected:

  // Does a small loop at HB.
  void RunLoopHB(Automata* aut) {
    AddEagerBlockTransition(Block_B231, Block_A139);
  }

  // goes down 149->179
  void RunDown(Automata* aut) {
    AddEagerBlockTransition(Block_A149, Block_A159);
    AddEagerBlockTransition(Block_A159, Block_A167);
    SwitchTurnout(Turnout_W159.b.magnet(), false);
    AddEagerBlockTransition(Block_A167, Block_A179);
    SwitchTurnout(Turnout_W170.b.magnet(), false);
  }

  // goes up 271->241
  void RunUp(Automata* aut) {
    AddEagerBlockTransition(Block_B271, Block_B261);
    SwitchTurnout(Turnout_W170.b.magnet(), false);
    AddEagerBlockTransition(Block_B261, Block_B251);
    SwitchTurnout(Turnout_W159.b.magnet(), false); // coupled
    AddEagerBlockTransition(Block_B251, Block_B241);
  }

  // Does a full loop of the large part of the layout (149->241)
  void RunLoopLayout(Automata* aut) {
    RunDown(aut);
    AddDirectBlockTransition(Block_A179, Block_B271, &g_not_paused_condition);
    SwitchTurnout(Turnout_W180.b.magnet(), true);
    RunUp(aut);
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
    RunLoopLayout(aut);
    {
      WithRouteLock l(this, &route_lock_HB);
      AddDirectBlockTransition(Block_B241, Block_B231, &g_not_paused_condition);
      SwitchTurnout(Turnout_HBW1.b.magnet(), MovableTurnout::kClosed);
      SwitchTurnout(Turnout_HBW3.b.magnet(), MovableTurnout::kClosed);
    }
    RunLoopHB(aut);
    {
      WithRouteLock l(this, &route_lock_HB);
      AddDirectBlockTransition(Block_A139, Block_A149, &g_hb_both_dkw_free_out);
      SwitchTurnout(DKW_HBW4.b.magnet(), DKW::kDKWStateCross);
      SwitchTurnout(DKW_HBW2.b.magnet(), DKW::kDKWStateCross);
    }
  }


  void RunAltCycle(Automata* aut) {
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

class IC2000Train : public LayoutSchedule {
 public:
  IC2000Train(const string& name, uint64_t train_id, uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    RunLoopLayout(aut);
    {
      WithRouteLock l(this, &route_lock_HB);
      AddDirectBlockTransition(Block_B241, Stub_HBB1, &g_hb_dkw2_free_in);
      SwitchTurnout(Turnout_HBW1.b.magnet(), MovableTurnout::kThrown);
      SwitchTurnout(DKW_HBW2.b.magnet(), DKW::kDKWStateCross);
      SwitchTurnout(Turnout_HBW6.b.magnet(), MovableTurnout::kClosed);
      StopAndReverseAtStub(Stub_HBB1);

      AddBlockTransitionOnPermit(Stub_HBB1.b_.rev_signal, Block_A149, &hb_fromb1, &g_hb_dkw2_free_out, false);
      SwitchTurnout(Turnout_HBW6.b.magnet(), MovableTurnout::kClosed);
      SwitchTurnout(DKW_HBW2.b.magnet(), DKW::kDKWStateCurved);
    }
  }
};

class PassengerTrainTrack2 : public LayoutSchedule {
 public:
  PassengerTrainTrack2(const string& name, uint64_t train_id,
                       uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    RunLoopLayout(aut);
    {
      WithRouteLock l(this, &route_lock_HB);
      AddDirectBlockTransition(Block_B241, Stub_HBB2, &g_hb_dkw2_free_in);
      SwitchTurnout(Turnout_HBW1.b.magnet(), MovableTurnout::kThrown);
      SwitchTurnout(DKW_HBW2.b.magnet(), DKW::kDKWStateCross);
      SwitchTurnout(Turnout_HBW6.b.magnet(), MovableTurnout::kThrown);
      StopAndReverseAtStub(Stub_HBB2);

      AddBlockTransitionOnPermit(Stub_HBB2.b_.rev_signal, Block_A149, &hb_fromb2, &g_hb_dkw2_free_out, false);
      SwitchTurnout(Turnout_HBW6.b.magnet(), MovableTurnout::kThrown);
      SwitchTurnout(DKW_HBW2.b.magnet(), DKW::kDKWStateCurved);
    }
  }
};

class PassengerTrainTrack4 : public LayoutSchedule {
 public:
  PassengerTrainTrack4(const string& name, uint64_t train_id,
                       uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    RunLoopLayout(aut);
    {
      WithRouteLock l(this, &route_lock_HB);
      AddDirectBlockTransition(Block_B241, Stub_HBB4, &g_hb_dkw4_free_in);
      SwitchTurnout(Turnout_HBW1.b.magnet(), MovableTurnout::kClosed);
      SwitchTurnout(Turnout_HBW3.b.magnet(), MovableTurnout::kThrown);
      SwitchTurnout(DKW_HBW4.b.magnet(), DKW::kDKWStateCross);
      SwitchTurnout(Turnout_HBW5.b.magnet(), MovableTurnout::kThrown);
      StopAndReverseAtStub(Stub_HBB4);

      AddBlockTransitionOnPermit(Stub_HBB4.b_.rev_signal, Block_A149, &hb_fromb4, &g_hb_both_dkw_free_out, false);
      SwitchTurnout(Turnout_HBW5.b.magnet(), MovableTurnout::kThrown);
      SwitchTurnout(DKW_HBW4.b.magnet(), DKW::kDKWStateCurved);
      SwitchTurnout(DKW_HBW2.b.magnet(), DKW::kDKWStateCross);
    }
  }
};


class IC2000TrainB : public LayoutSchedule {
 public:
  IC2000TrainB(const string& name, uint64_t train_id, uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    /*RunXtoY(aut);
    RunStubYY(aut);
    RunYtoX(aut);
    RunStubXX(aut);*/
  }
};

class FreightTrain : public LayoutSchedule {
 public:
  FreightTrain(const string& name, uint64_t train_id, uint8_t default_speed)
      : LayoutSchedule(name, train_id, default_speed) {}

  void RunTransition(Automata* aut) OVERRIDE {
    RunCycle(aut);
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

IC2000Train train_re460tsr("Re460-TSR", DccShortAddress(22), 35);
// TCS ACS64 with builtin keepalive
PassengerTrainTrack2 train_ice2("ICE2", DccShortAddress(40), 15);
// ESU LokPilot Standard with Keepalive TCS
PassengerTrainTrack4 train_re465("Re465", DccShortAddress(47), 25);
IC2000TrainB train_icn("ICN", DccShortAddress(50), 13);
IC2000TrainB train_bde44("BDe-4/4", DccShortAddress(38), 35);
FreightTrain train_re620("Re620", DccShortAddress(5), 45);
FreightTrain train_re420("Re420", DccShortAddress(4), 45);
FreightTrain train_rheingold("Rheingold", MMAddress(19), 35);
FreightTrain train_rts("rts_railtraction", DccShortAddress(32), 20);
FreightTrain train_wle("wle_er20", DccShortAddress(27), 20);
FreightTrain train_re474("Re474", MMAddress(12), 30);
FreightTrain train_re460hag("Re460_HAG", DccShortAddress(26), 32);
FreightTrain train_br290("BR290", MMAddress(29), 30);
FreightTrain train_krokodil("Krokodil", MMAddress(68), 30);

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
