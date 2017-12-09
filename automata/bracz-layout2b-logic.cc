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

EventBasedVariable route_lock_XX(&brd, "route_lock_XX", BRACZ_LAYOUT | 0x0014,
                                 BRACZ_LAYOUT | 0x0015, 7, 30, 3);

EventBasedVariable route_lock_YY(&brd, "route_lock_YY", BRACZ_LAYOUT | 0x0016,
                                 BRACZ_LAYOUT | 0x0017, 7, 30, 2);


EventBasedVariable estop_short(&brd, "estop_short", BRACZ_LAYOUT | 0x0018,
                               BRACZ_LAYOUT | 0x0019, 7, 30, 1);

EventBasedVariable estop_nopwr(&brd, "estop", 0x010000000000FFFFULL,
                               0x010000000000FFFEULL, 7, 30, 0);

EventBasedVariable global_dispatch(&brd, "global_dispatch",
                                   BRACZ_LAYOUT | 0x001A, BRACZ_LAYOUT | 0x001B,
                                   7, 29, 7);

EventBasedVariable runaround_olten(&brd, "runaround_olten",
                                   BRACZ_LAYOUT | 0x001C, BRACZ_LAYOUT | 0x001D,
                                   7, 29, 6);

EventBasedVariable runaround_daniken(&brd, "runaround_daniken",
                                   BRACZ_LAYOUT | 0x001E, BRACZ_LAYOUT | 0x001F,
                                   7, 29, 5);

EventBasedVariable runaround_lenzburg(&brd, "runaround_lenzburg",
                                   BRACZ_LAYOUT | 0x0020, BRACZ_LAYOUT | 0x0021,
                                   7, 29, 4);

EventBasedVariable runaround_spreitenbach(&brd, "runaround_spreitenbach",
                                   BRACZ_LAYOUT | 0x0022, BRACZ_LAYOUT | 0x0023,
                                   7, 29, 3);

EventBasedVariable runaround_rbl(&brd, "runaround_rbl",
                                   BRACZ_LAYOUT | 0x0024, BRACZ_LAYOUT | 0x0025,
                                   7, 29, 2);

EventBasedVariable runaround_yard(&brd, "runaround_yard",
                                   BRACZ_LAYOUT | 0x0026, BRACZ_LAYOUT | 0x0027,
                                   7, 29, 1);

// I2CBoard b5(0x25), b6(0x26); //, b7(0x27), b1(0x21), b2(0x22);
//NativeIO n9(0x29);
AccBoard ba(0x2a), bb(0x2b), bc(0x2c), bd(0x2d), be(0x2e);

/*StateRef StGreen(2);
StateRef StGoing(3);
*/

StateRef StUser1(10);
StateRef StUser2(11);


// This is one double-bridge
I2CSignal signal_A469_adv(&bb, 19, "A469.adv");
I2CSignal signal_A369_adv(&bb, 17, "A369.adv");

I2CSignal signal_B469_main(&bb, 14, "B469.main");
I2CSignal signal_B469_adv(&bb, 15, "B469.adv");

I2CSignal signal_B369_main(&bb, 12, "B369.main");
I2CSignal signal_B369_adv(&bb, 13, "B369.adv");
// end bridge

I2CSignal signal_A461_main(&bb, 72, "A461.main");
I2CSignal signal_A461_adv(&bb, 73, "A461.adv");

I2CSignal signal_A361_main(&bb, 34, "A361.main");
I2CSignal signal_A361_adv(&bb, 35, "A361.adv");

// 4-way bridge. Heads are from left to right.
I2CSignal signal_XXA1_main(&bb, 53, "XX.A1.main");
I2CSignal signal_XXA1_adv(&bb, 54, "XX.A1.adv");
I2CSignal signal_XXA2_main(&bb, 55, "XX.A2.main");
I2CSignal signal_XXA2_adv(&bb, 56, "XX.A2.adv");
I2CSignal signal_XXA3_main(&bb, 57, "XX.A3.main");
I2CSignal signal_XXA3_adv(&bb, 58, "XX.A3.adv");
I2CSignal signal_XXA4_main(&bb, 65, "XX.A4.main");
I2CSignal signal_XXA4_adv(&bb, 66, "XX.A4.adv");
// end four-way bridge.

// 2-way bridge. This has main signals on both sides. The first head is
// the one with the wires connected, then goes left-to-right and then other
// side left-to-right.
I2CSignal signal_A441_main(&bc, 36, "A441.main");
I2CSignal signal_A441_adv(&bc, 37, "A441.adv");
I2CSignal signal_A341_main(&bc, 42, "A341.main");
I2CSignal signal_A341_adv(&bc, 43, "A341.adv");
I2CSignal signal_B339_main(&bc, 40, "B339.main");
I2CSignal signal_B339_adv(&bc, 41, "B339.adv");
I2CSignal signal_B439_main(&bc, 38, "B439.main");
I2CSignal signal_B439_adv(&bc, 39, "B439.adv");
// End bridge.

// 2-way bridge with main signals on one side, advance on the other. Heads are
// from left to right (starting at main head).
I2CSignal signal_A431_main(&bd, 6, "A431.main");
I2CSignal signal_A431_adv(&bd, 7, "A431.adv");
I2CSignal signal_A331_main(&bd, 32, "A331.main");
I2CSignal signal_A331_adv(&bd, 33, "A331.adv");
I2CSignal signal_B331_adv(&bd, 75, "B331.adv");
I2CSignal signal_B431_adv(&bd, 74, "B431.adv");
// End bridge

// Lone main-only head with a male-female plug.
I2CSignal signal_YYB12_main(&bd, 62, "YY.B12.main");
// Lone main-only head with a male plug.
I2CSignal signal_YYB22_main(&bd, 63, "YY.B22.main");
// Double wired main-only head. Shorter wire:
I2CSignal signal_YYB42_main(&bd, 25, "YY.B42.main"); // yard lead
// Longer wire
I2CSignal signal_YYB32_main(&bd, 24, "YY.B32.main"); // staging exit

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

I2CSignal signal_B349_main(&bc, 26, "B349.main");
I2CSignal signal_B349_adv(&bc, 27, "B349.adv");
I2CSignal signal_B449_main(&bc, 8, "B449.main");
I2CSignal signal_B449_adv(&bc, 9, "B449.adv");

I2CSignal signal_YYA13_main(&ba, 22, "YY.A13.main");
I2CSignal signal_YYA13_adv(&ba, 23, "YY.A13.adv");
I2CSignal signal_YYA23_main(&ba, 44, "YY.A23.main");
I2CSignal signal_YYA23_adv(&ba, 45, "YY.A23.adv");

/*
I2CSignal signal_A240_main(&ba, 26, "A240.main");
I2CSignal signal_A240_adv(&ba, 27, "A240.adv");

I2CSignal signal_A131_main(&ba, 8, "A131.main");
I2CSignal signal_A131_adv(&ba, 9, "A131.adv");

I2CSignal signal_B129_main(&ba, 53, "B129.main");
I2CSignal signal_B129_adv(&ba, 54, "B129.adv");

I2CSignal signal_B229_main(&ba, 55, "B229.main");
I2CSignal signal_B229_adv(&ba, 56, "B229.adv");

I2CSignal signal_B329_main(&ba, 57, "B329.main");
I2CSignal signal_B329_adv(&ba, 58, "B329.adv");

I2CSignal signal_B429_main(&ba, 65, "B429.main");
I2CSignal signal_B429_adv(&ba, 66, "B429.adv");

I2CSignal signal_A317_main(&bb, 63, "A317.main");
I2CSignal signal_A217_main(&bb, 24, "A217.main");
I2CSignal signal_A117_main(&bb, 25, "A117.main");


I2CSignal signal_B108_main(&bb, 12, "B108.main");
I2CSignal signal_B108_adv(&bb, 13, "B108.adv");
I2CSignal signal_B208_main(&bb, 14, "B208.main");
I2CSignal signal_B208_adv(&bb, 15, "B208.adv");

I2CSignal signal_A108_adv(&bb, 17, "A108.adv");
I2CSignal signal_A208_adv(&bb, 19, "A208.adv");

I2CSignal signal_B400_main(&bb, 4, "B400.main");
I2CSignal signal_B400_adv(&bb, 5, "B400.adv");

I2CSignal signal_A100_main(&be, 32, "A100.main");
I2CSignal signal_A100_adv(&be, 33, "A100.adv");
I2CSignal signal_A200_main(&be, 6, "A200.main");
I2CSignal signal_A200_adv(&be, 7, "A200.adv");

I2CSignal signal_B100_adv(&be, 75, "B100.adv");
I2CSignal signal_B200_adv(&be, 74, "B200.adv");

I2CSignal signal_XXB1_main(&be, 69, "XX.B1.main");
I2CSignal signal_XXB1_adv(&be, 70, "XX.B1.adv");
I2CSignal signal_XXB2_main(&be, 67, "XX.B2.main");
I2CSignal signal_XXB2_adv(&be, 68, "XX.B2.adv");
I2CSignal signal_XXB3_main(&be, 51, "XX.B3.main");
I2CSignal signal_XXB3_adv(&be, 52, "XX.B3.adv");
I2CSignal signal_XXB4_main(&be, 49, "XX.B4.main");
I2CSignal signal_XXB4_adv(&be, 50, "XX.B4.adv");

I2CSignal signal_A406_main(&be, 72, "A406.main");
I2CSignal signal_A406_adv(&be, 73, "A406.adv");

I2CSignal signal_A700_main(&be, 22, "A700.main");
I2CSignal signal_A700_adv(&be, 23, "A700.adv");

I2CSignal signal_XXA1_main(&bc, 31, "XX.A1.main");
I2CSignal signal_XXA1_adv(&bc, 32, "XX.A1.adv");
I2CSignal signal_XXA2_main(&bc, 44, "XX.A2.main");
I2CSignal signal_XXA2_adv(&bc, 45, "XX.A2.adv");
I2CSignal signal_XXA3_main(&bc, 10, "XX.A3.main");
I2CSignal signal_XXA3_adv(&bc, 11, "XX.A3.adv");
*/

/* More signals

PandaControlBoard panda_bridge;

LPC11C lpc11_back;

I2CSignal signal_XXB2_main(&ba, 31, "XX.B2.main");  // 159 (0x9F)
I2CSignal signal_XXB2_adv(&ba, 32, "XX.B2.adv");

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

I2CSignal signal_B447_main(&bd, 10, "B447.main");  // 131 0x83
I2CSignal signal_B447_adv(&bd, 11, "B447.adv");
I2CSignal signal_A447_main(&bd, 44, "A447.main");  // 151 0x97
I2CSignal signal_A447_adv(&bd, 45, "A447.adv");

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

I2CSignal signal_YYB2_main(&be, 4, "YY.B2.main");  // 141 (0x8D)
I2CSignal signal_YYB2_adv(&be, 5, "YY.B2.adv");

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

  DefCopy(*rep, ImportVariable(&ba.LedBlueSw));
  DefCopy(*rep, ImportVariable(&bb.LedBlueSw));
  DefCopy(*rep, ImportVariable(&bc.LedBlueSw));
  DefCopy(*rep, ImportVariable(&bd.LedBlueSw));
  DefCopy(*rep, ImportVariable(&be.LedBlueSw));
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
const AllocatorPtr& logic2(logic);
const AllocatorPtr& train_perm(perm.allocator());

FlipFlopAutomata xx_flipflop(&brd, "xx_flipflop", logic, 32);
FlipFlopClient xx_tob1("to_B1", &xx_flipflop);
FlipFlopClient xx_tob2("to_B2", &xx_flipflop);
FlipFlopClient xx_tob3("to_B3", &xx_flipflop);
FlipFlopClient xx_tob4("to_B4", &xx_flipflop);

FlipFlopAutomata xe_flipflop(&brd, "xe_flipflop", logic, 32);
FlipFlopClient xe_fromb1("from_B1", &xe_flipflop);
FlipFlopClient xe_fromb2("from_B2", &xe_flipflop);
FlipFlopClient xe_fromb3("from_B3", &xe_flipflop);
FlipFlopClient xe_fromb4("from_B4", &xe_flipflop);


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

AllocatorPtr train_tmp(logic2->Allocate("train", 768));

MagnetCommandAutomata g_magnet_aut(&brd, logic2);
MagnetPause magnet_pause(&g_magnet_aut, &power_acc);
MagnetButtonAutomata g_btn_aut(&brd);


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

MagnetDef Magnet_XXW3(&g_magnet_aut, "XX.W3", &bb.ActGreenGreen,
                      &bb.ActGreenRed, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_XXW3(&brd,
                                    logic->Allocate("XX.W3", 40),
                                    &Magnet_XXW3);
TurnoutWrap TXXW3(&Turnout_XXW3.b, kPointToClosed);

MagnetDef Magnet_XXW4(&g_magnet_aut, "XX.W4", &bb.ActOraGreen,
                      &bb.ActOraRed, MovableTurnout::kThrown);
StandardMovableTurnout Turnout_XXW4(&brd,
                                    logic->Allocate("XX.W4", 40),
                                    &Magnet_XXW4);
TurnoutWrap TXXW4(&Turnout_XXW4.b, kPointToThrown);


CoupledMagnetDef Magnet_XXW5(&g_magnet_aut, "XX.W5", &Magnet_XXW3, false);
StandardMovableTurnout Turnout_XXW5(&brd,
                                    logic->Allocate("XX.W5", 40),
                                    &Magnet_XXW5);
TurnoutWrap TXXW5(&Turnout_XXW5.b, kPointToClosed);

MagnetDef Magnet_XXW6(&g_magnet_aut, "XX.W6", &be.ActGreenGreen,
                      &be.ActGreenRed, MovableTurnout::kClosed);
StandardMovableTurnout Turnout_XXW6(&brd,
                                    logic->Allocate("XX.W6", 40),
                                    &Magnet_XXW6);
TurnoutWrap TXXW6(&Turnout_XXW6.b, kClosedToPoint);

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

PhysicalSignal YYA33(&be.InOraRed, nullptr, nullptr, nullptr,
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

StubBlock Stub_XXB1(&brd, &XXB1, nullptr, logic, "XX.B1");
StubBlock Stub_XXB2(&brd, &XXB2, nullptr, logic, "XX.B2");
StubBlock Stub_XXB3(&brd, &XXB3, nullptr, logic, "XX.B3");
StubBlock Stub_XXB4(&brd, &XXB4, nullptr, logic, "XX.B4");

StandardBlock Block_A461(&brd, &A461, logic, "A461");
StandardBlock Block_B369(&brd, &B369, logic, "B369");

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
StubBlock Stub_YYA4(&brd, &YYA4, nullptr, logic, "YY.A4");

StubBlock Stub_YYA33(&brd, &YYA33, nullptr, logic, "YY.A33");
StandardBlock Block_YYB42(&brd, &YYB42, logic, "YY.B42");


bool ignored1 = BindPairs({
    //
    {Stub_XXB1.entry(), Turnout_XXW6.b.side_thrown()},
    {Stub_XXB3.entry(), Turnout_XXW4.b.side_closed()},
    {Turnout_XXW5.b.side_thrown(), Turnout_XXW3.b.side_thrown()},
    {Turnout_XXW1.b.side_closed(), Turnout_XXW2.b.side_thrown()},
    {Turnout_YYW6.b.side_thrown(), Stub_YYA4.entry()},
    {Turnout_YYW7.b.side_thrown(), Stub_YYA3.entry()},
    {Turnout_YYW8.b.side_thrown(), Stub_YYA2.entry()},
    {Turnout_YYW1.b.side_closed(), Turnout_YYW2.b.side_thrown()},
    {Turnout_YYW9.b.side_closed(), Stub_YYA33.entry()},
    {Turnout_W340.b.side_thrown(), Turnout_W440.b.side_closed()} //
});

bool ignored2 = BindSequence(  //
    Stub_XXB2.entry(),         //
    {&TXXW6, &TXXW5, &TXXW1, &Block_A461, &TW360, &Mdl450, &TW349, &Block_A441,
     &TW440, &Block_A431, &TYYW1, &Block_YYA13, &TYYW4, &TYYW5, &TYYW6, &TYYW7,
     &TYYW8},
    Stub_YYA1.entry());

bool ignored4 = BindSequence(  //
    Turnout_YYW4.b.side_thrown(),
    {&Block_YYB22, &TYYW3, &TYYW2, &Block_B339, &TW340, &Block_B349},
    Turnout_W349.b.side_thrown());

bool ignored3 = BindSequence(
    Turnout_W360.b.side_thrown(),
    {&Block_B369, &TXXW2, &TXXW3, &TXXW4},
    Stub_XXB4.entry());

bool ignored5 = BindSequence(  //
    Turnout_YYW5.b.side_closed(),
    {&Block_YYB42, &TYYW9},
    Turnout_YYW3.b.side_thrown());


auto& Block_EntryToXX = Block_B369;
auto& Block_ExitFromXX = Block_A461;
auto& Block_EntryToYY = Block_YYA13;
auto& Block_ExitFromYY = Block_YYB22;





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
  BlockSignal(this, &Stub_XXB4.b_, global_dispatch);
});

DefAut(signalaut2, brd, {
  BlockSignal(this, &Stub_YYA1.b_, runaround_yard);
  BlockSignal(this, &Stub_YYA2.b_, runaround_yard);
  ClearUsedVariables();
  BlockSignal(this, &Stub_YYA3.b_, runaround_yard);
  BlockSignal(this, &Stub_YYA4.b_, runaround_yard);
  ClearUsedVariables();
  BlockSignal(this, &Stub_YYA33.b_, runaround_rbl);
  BlockSignal(this, &Block_YYB42, runaround_rbl);
});

DefAut(signalaut3, brd, {
  ClearUsedVariables();
});

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

void IfYYExitFree(Automata::Op* op) {
  IfNotPaused(op);
  IfNotShutdown(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_YYW6.b.any_route()));
}
auto g_yy_exit_free = NewCallback(&IfYYExitFree);


void If355Free(Automata::Op* op) {
  IfNotPaused(op);
  op->IfReg0(op->parent()->ImportVariable(*Turnout_W360.b.any_route()));
}
auto g_355_free = NewCallback(&If355Free);


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
  void RunB108_to_A240(Automata* aut) {
    AddEagerBlockTransition(Block_B108, Block_B129);
    AddEagerBlockTransition(Block_B129, Block_A240);
    }*/


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

    AddBlockTransitionOnPermit(Block_EntryToXX, Stub_XXB4, &xx_tob4, &g_xx12_entry_free, false);
    SwitchTurnout(Turnout_XXW3.b.magnet(), false);
    SwitchTurnout(Turnout_XXW4.b.magnet(), true);

    StopAndReverseAtStub(Stub_XXB4);

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
    AddBlockTransitionOnPermit(Stub_XXB4.b_.rev_signal, Block_ExitFromXX,
                               &xe_fromb4, &g_xx12_exit_free);
    SwitchTurnout(Turnout_XXW2.b.magnet(), true);
    AddBlockTransitionOnPermit(Stub_XXB2.b_.rev_signal, Block_ExitFromXX,
                               &xe_fromb2, &g_xx34_exit_free);
    SwitchTurnout(Turnout_XXW5.b.magnet(), false);
    AddBlockTransitionOnPermit(Stub_XXB1.b_.rev_signal, Block_ExitFromXX,
                               &xe_fromb1, &g_xx34_exit_free);
    SwitchTurnout(Turnout_XXW5.b.magnet(), false);
  }

  void RunStubYY(Automata* aut) {
    WithRouteLock l(this, &route_lock_YY);
    AddBlockTransitionOnPermit(Block_EntryToYY, Stub_YYA4, &yy_toa4, &g_yy_entry_free);
    SwitchTurnout(Turnout_YYW6.b.magnet(), true);
    
    StopAndReverseAtStub(Stub_YYA4);

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

    AddBlockTransitionOnPermit(Stub_YYA4.b_.rev_signal,
                               Block_ExitFromYY,
                               &ye_froma4,
                               &g_yy_exit_free);
    SwitchTurnout(Turnout_YYW4.b.magnet(), true);

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

  void RunCycle(Automata* aut) {
  /*
    RunB108_to_A240(aut);

    AddBlockTransitionOnPermit(Block_A240, Block_A217, &l240_to217, &g_not_paused_condition, true);
    SwitchTurnout(Turnout_W231.b.magnet(), false);

    AddBlockTransitionOnPermit(Block_A240, Block_A317, &l240_to317, &g_not_paused_condition, true);
    SwitchTurnout(Turnout_W231.b.magnet(), true);

    {
      WithRouteLock l(this, &route_lock_WW);
      AddBlockTransitionOnPermit(Block_A217, Block_A406, &w217_from217, &g_dkw209_free);
      SwitchTurnout(DKW_W216.b.magnet(), MovableDKW::kDKWStateCross);
      SwitchTurnout(DKW_W209.b.magnet(), MovableDKW::kDKWStateCurved);

      AddBlockTransitionOnPermit(Block_A317, Block_A406, &w217_from317, &g_dkw209_free);
      SwitchTurnout(DKW_W216.b.magnet(), MovableDKW::kDKWStateCurved);
      SwitchTurnout(DKW_W209.b.magnet(), MovableDKW::kDKWStateCurved);
    }

    AddEagerBlockTransition(Block_A406, Block_XXB1);
    AddDirectBlockTransition(Block_XXB1, Block_B108, &g_not_paused_condition);
  */
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

IC2000TrainB train_re460tsr("Re460-TSR", DccShortAddress(22), 35);
IC2000Train train_ice2("ICE2", MMAddress(2), 15);
IC2000Train train_re465("Re465", DccShortAddress(47), 25);
IC2000Train train_icn("ICN", DccShortAddress(50), 13);
IC2000Train train_bde44("BDe-4/4", DccShortAddress(38), 35);


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
