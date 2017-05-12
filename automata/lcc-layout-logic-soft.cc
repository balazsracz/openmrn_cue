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
StateRef StUser1(10);
StateRef StUser2(11);

#define OUTPUT_BASE 0x0501010118010100
#define INPUT_BASE 0x0501010118010000

#define V_BASE 0x0501010118010200

EventBasedVariable block_main1(&brd, "main1", INPUT_BASE | 0x00,
                               INPUT_BASE | 01, 0, OFS_IOA, 0);
EventBasedVariable block_main2(&brd, "main2", INPUT_BASE | 0x1E,
                               INPUT_BASE | (0x1E + 1), 0, OFS_IOA, 1);
EventBasedVariable block_main3(&brd, "main3", INPUT_BASE | 0x02,
                               INPUT_BASE | (0x02 + 1), 0, OFS_IOA, 2);
EventBasedVariable block_main4(&brd, "main4", INPUT_BASE | 0x0A,
                               INPUT_BASE | (0x0A + 1), 0, OFS_IOA, 3);
EventBasedVariable block_main5(&brd, "main5", INPUT_BASE | 0x06,
                               INPUT_BASE | (0x06 + 1), 0, OFS_IOA, 4);
EventBasedVariable block_siding1(&brd, "siding1", INPUT_BASE | 0x04,
                                 INPUT_BASE | (0x04 + 1), 0, OFS_IOA, 5);
EventBasedVariable block_siding2(&brd, "siding2", INPUT_BASE | 0x08,
                                 INPUT_BASE | (0x08 + 1), 0, OFS_IOA, 6);

EventBasedVariable block_stub1(&brd, "stub1", INPUT_BASE | 0x18,
                               INPUT_BASE | (0x18 + 1), 0, OFS_IOA + 1, 0);
EventBasedVariable block_stub2(&brd, "stub2", INPUT_BASE | 0x1C,
                               INPUT_BASE | (0x1C + 1), 0, OFS_IOA + 1, 1);

EventBasedVariable turnout_stub1(&brd, "turnout_stub1", INPUT_BASE | 0x4C,
                                 INPUT_BASE | 0x4E, 0, OFS_IOB, 0);
EventBasedVariable turnout_stub2(&brd, "turnout_stub2", INPUT_BASE | 0x2E,
                                 INPUT_BASE | 0x2C, 0, OFS_IOB, 1);
EventBasedVariable turnout_siding1(&brd, "turnout_siding1", INPUT_BASE | 0x0C,
                                   INPUT_BASE | 0x0E, 0, OFS_IOB, 2);
EventBasedVariable turnout_siding2(&brd, "turnout_siding2", INPUT_BASE | 0x3C,
                                   INPUT_BASE | 0x3E, 0, OFS_IOB, 3);

EventBlock logic(&brd, BRACZ_LAYOUT | 0xE000, "logic");

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

std::unique_ptr<GlobalVariable> rrx1(logic.allocator()->Allocate("rr_x1"));
EventBasedVariable out_x1a(&brd, "rrx1_a", OUTPUT_BASE | 0x0, OUTPUT_BASE | 0x1,
                           2, OFS_IOB, 0);
EventBasedVariable out_x1b(&brd, "rrx1_b", OUTPUT_BASE | 0x2, OUTPUT_BASE | 0x3,
                           2, OFS_IOB, 1);

std::unique_ptr<GlobalVariable> rrx2(logic.allocator()->Allocate("rr_x2"));
EventBasedVariable out_x2a(&brd, "rrx2_a", OUTPUT_BASE | 0x4, OUTPUT_BASE | 0x5,
                           2, OFS_IOB, 2);
EventBasedVariable out_x2b(&brd, "rrx2_b", OUTPUT_BASE | 0x6, OUTPUT_BASE | 0x7,
                           2, OFS_IOB, 3);

EventBasedVariable signal_main3_red(&brd, "sig_main3_red", OUTPUT_BASE | 0x1C,
                                    OUTPUT_BASE | 0x1D, 1, OFS_IOA, 0);
EventBasedVariable signal_main3_green(&brd, "sig_main3_green",
                                      OUTPUT_BASE | 0x1E, OUTPUT_BASE | 0x1F, 1,
                                      OFS_IOA, 1);

EventBasedVariable signal_main2_top_red(&brd, "sig_main2_top_red",
                                        OUTPUT_BASE | 0x14, OUTPUT_BASE | 0x15,
                                        1, OFS_IOA, 2);
EventBasedVariable signal_main2_top_green(&brd, "sig_main2_top_green",
                                          OUTPUT_BASE | 0x16,
                                          OUTPUT_BASE | 0x17, 1, OFS_IOA, 3);
EventBasedVariable signal_main2_bot_red(&brd, "sig_main2_bot_red",
                                        OUTPUT_BASE | 0x10, OUTPUT_BASE | 0x11,
                                        1, OFS_IOA, 4);
EventBasedVariable signal_main2_bot_green(&brd, "sig_main2_bot_green",
                                          OUTPUT_BASE | 0x12,
                                          OUTPUT_BASE | 0x13, 1, OFS_IOA, 5);

EventBasedVariable signal_main5_to_4_red(&brd, "sig_main5_to_4_red",
                                         OUTPUT_BASE | 0x4C, OUTPUT_BASE | 0x4D,
                                         1, OFS_IOA, 6);
EventBasedVariable signal_main5_to_4_green(&brd, "sig_main5_to_4_green",
                                           OUTPUT_BASE | 0x4E,
                                           OUTPUT_BASE | 0x4F, 1, OFS_IOA, 7);

EventBasedVariable signal_siding1_red(&brd, "sig_siding1_red",
                                      OUTPUT_BASE | 0x48, OUTPUT_BASE | 0x49, 1,
                                      OFS_IOA + 1, 0);
EventBasedVariable signal_siding1_green(&brd, "sig_siding1_green",
                                        OUTPUT_BASE | 0x4A, OUTPUT_BASE | 0x4B,
                                        1, OFS_IOA + 1, 1);

EventBasedVariable signal_main5_to_1_red(&brd, "sig_main5_to_1_red",
                                         OUTPUT_BASE | 0x24, OUTPUT_BASE | 0x25,
                                         1, OFS_IOA + 1, 2);
EventBasedVariable signal_main5_to_1_green(&brd, "sig_main5_to_1_green",
                                           OUTPUT_BASE | 0x26,
                                           OUTPUT_BASE | 0x27, 1, OFS_IOA + 1,
                                           3);

EventBasedVariable signal_siding2_red(&brd, "sig_siding2_red",
                                      OUTPUT_BASE | 0x20, OUTPUT_BASE | 0x21, 1,
                                      OFS_IOA + 1, 4);
EventBasedVariable signal_siding2_green(&brd, "sig_siding2_green",
                                        OUTPUT_BASE | 0x22, OUTPUT_BASE | 0x23,
                                        1, OFS_IOA + 1, 5);

EventBasedVariable signal_main1_top_red(&brd, "sig_main1_top_red",
                                        OUTPUT_BASE | 0x2C, OUTPUT_BASE | 0x2D,
                                        1, OFS_IOA + 1, 6);
EventBasedVariable signal_main1_top_green(&brd, "sig_main1_top_green",
                                          OUTPUT_BASE | 0x2A,
                                          OUTPUT_BASE | 0x2B, 1, OFS_IOA + 1,
                                          7);
EventBasedVariable signal_main1_bot_red(&brd, "sig_main1_bot_red",
                                        OUTPUT_BASE | 0x28, OUTPUT_BASE | 0x29,
                                        1, OFS_IOB, 0);
EventBasedVariable signal_main1_bot_green(&brd, "sig_main1_bot_green",
                                          OUTPUT_BASE | 0x2E,
                                          OUTPUT_BASE | 0x2F, 1, OFS_IOB, 1);

EventBasedVariable signal_main4_top_red(&brd, "sig_main4_top_red",
                                        OUTPUT_BASE | 0x40, OUTPUT_BASE | 0x41,
                                        1, OFS_IOB, 2);
EventBasedVariable signal_main4_top_green(&brd, "sig_main4_top_green",
                                          OUTPUT_BASE | 0x42,
                                          OUTPUT_BASE | 0x43, 1, OFS_IOB, 3);
EventBasedVariable signal_main4_bot_red(&brd, "sig_main4_bot_red",
                                        OUTPUT_BASE | 0x44, OUTPUT_BASE | 0x45,
                                        1, OFS_IOB, 4);
EventBasedVariable signal_main4_bot_green(&brd, "sig_main4_bot_green",
                                          OUTPUT_BASE | 0x46,
                                          OUTPUT_BASE | 0x47, 1, OFS_IOB, 5);

EventBasedVariable req_cw(&brd, "request_cw", V_BASE | 0, V_BASE | 1, 2,
                          OFS_IOA, 0);

EventBasedVariable go_cw(&brd, "go_cw", V_BASE | 3, V_BASE | 2, 2, OFS_IOA, 1);

EventBasedVariable req_ccw(&brd, "request_ccw", V_BASE | 4, V_BASE | 4 | 1, 2,
                           OFS_IOA, 2);

EventBasedVariable go_ccw(&brd, "go_ccw", V_BASE | 7, V_BASE | 4 | 2, 2,
                          OFS_IOA, 3);

EventBasedVariable req_stop(&brd, "request_stop", V_BASE | 8, V_BASE | 8 | 1, 2,
                            OFS_IOA, 4);

EventBasedVariable go_stop(&brd, "go_stop", V_BASE | 11, V_BASE | 8 | 2, 2,
                           OFS_IOA, 5);

EventBasedVariable req_off(&brd, "request_off", V_BASE | 12, V_BASE | 12 | 1, 2,
                           OFS_IOA, 6);

EventBasedVariable go_off(&brd, "go_off", V_BASE | 15, V_BASE | 12 | 2, 2,
                          OFS_IOA, 7);

#define SW_EVENT_BASE 0x05010101143F0000ULL

EventBasedVariable reset_sw(&brd, "resetsw", SW_EVENT_BASE + 0,
                            SW_EVENT_BASE + 1, 1, OFS_IOB, 6);

EventBasedVariable user_sw2(&brd, "sw2", SW_EVENT_BASE + 2, SW_EVENT_BASE + 3,
                            1, OFS_IOB, 7);

std::unique_ptr<GlobalVariable> constant_false(
    logic.allocator()->Allocate("constant_false"));

typedef automata::Automata::LocalVariable LocalVariable;

void CrossingBlink(Automata* aut, const LocalVariable& in_x,
                   LocalVariable* out_xa, LocalVariable* out_xb) {
  Def().IfState(StBase).IfReg1(in_x).ActState(StUser1);
  Def()
      .IfState(StUser1)
      .IfTimerDone()
      .ActState(StUser2)
      .ActReg1(out_xa)
      .ActReg0(out_xb)
      .ActTimer(1);
  Def()
      .IfState(StUser2)
      .IfTimerDone()
      .ActState(StUser1)
      .ActReg0(out_xa)
      .ActReg1(out_xb)
      .ActTimer(1);
  Def().IfReg0(in_x).ActState(StBase).ActReg1(out_xa).ActReg1(out_xb);
}

DefAut(crossing1, brd, {
  Def().IfState(StInit).ActState(StBase);
  auto* out_xa = ImportVariable(&out_x1a);
  auto* out_xb = ImportVariable(&out_x1b);
  auto* in_x = ImportVariable(rrx1.get());
  CrossingBlink(aut, *in_x, out_xa, out_xb);

  Def()
      .IfReg0(ImportVariable(block_main2))
      .IfReg0(ImportVariable(block_main3))
      .ActReg0(in_x);
  Def().IfReg1(ImportVariable(block_main2)).ActReg1(in_x);
  Def().IfReg1(ImportVariable(block_main3)).ActReg1(in_x);
});

DefAut(crossing2, brd, {
  Def().IfState(StInit).ActState(StBase);
  auto* out_xa = ImportVariable(&out_x2a);
  auto* out_xb = ImportVariable(&out_x2b);
  auto* in_x = ImportVariable(rrx2.get());
  CrossingBlink(aut, *in_x, out_xa, out_xb);

  Def()
      .IfReg0(ImportVariable(block_main4))
      .IfReg0(ImportVariable(block_main5))
      .IfReg0(ImportVariable(block_siding1))
      .ActReg0(in_x);
  Def().IfReg1(ImportVariable(block_main4)).ActReg1(in_x);
  Def().IfReg1(ImportVariable(block_main5)).ActReg1(in_x);
  Def().IfReg1(ImportVariable(block_siding1)).ActReg1(in_x);
});

// Logic OR of occupancy of main1 and main2
std::unique_ptr<GlobalVariable> agg_main12(
    logic.allocator()->Allocate("block_agg_main12"));
// Logic OR of occupancy of main3 and main4
std::unique_ptr<GlobalVariable> agg_main34(
    logic.allocator()->Allocate("block_agg_main34"));

// tells occupancy of siding2 when coming from main1, taking into account the
// state of turnout_stub2 and the occupancy state of stub2 and siding1,
// respectively.
std::unique_ptr<GlobalVariable> agg_siding2_from_main1(
    logic.allocator()->Allocate("block_agg_siding2_from_main1"));

// std::unique_ptr<GlobalVariable> agg_siding1_from_main4(
//    logic.allocator()->Allocate("block_agg_siding1_from_main4"));

void OccupancyAggregate(Automata* aut, const LocalVariable& block1,
                        const LocalVariable& block2, LocalVariable* block_out) {
  Def().IfReg0(block1).IfReg0(block2).ActReg0(block_out);
  Def().IfReg1(block1).ActReg1(block_out);
  Def().IfReg1(block2).ActReg1(block_out);
}

DefAut(block_agg, brd, {
  OccupancyAggregate(aut, ImportVariable(block_main1),
                     ImportVariable(block_main2),
                     ImportVariable(agg_main12.get()));
  OccupancyAggregate(aut, ImportVariable(block_main3),
                     ImportVariable(block_main4),
                     ImportVariable(agg_main34.get()));
});

// These variables tell whether the next logical block is occupied (1) or
// free(0), where next is defined by the turnout setting.
std::unique_ptr<GlobalVariable> next_main4(
    logic.allocator()->Allocate("next_main4"));
std::unique_ptr<GlobalVariable> next_main2(
    logic.allocator()->Allocate("next_main2"));
std::unique_ptr<GlobalVariable> next_main1(
    logic.allocator()->Allocate("next_main1"));
std::unique_ptr<GlobalVariable> next_siding2(
    logic.allocator()->Allocate("next_siding2"));

// These variables are outputs of the turnout-conditional ABS, and tell zero if
// the turnout is not lined up with the current place, and tell the occupancy
// of the next block ifthe turnout is lined up.
std::unique_ptr<GlobalVariable> condnext_main5_to_4(
    logic.allocator()->Allocate("condnext_main5_to_4"));
std::unique_ptr<GlobalVariable> condnext_siding1_to_main4(
    logic.allocator()->Allocate("condnext_siding1_to_main4"));

std::unique_ptr<GlobalVariable> condnext_main5_to_1(
    logic.allocator()->Allocate("condnext_main5_to_1"));
std::unique_ptr<GlobalVariable> condnext_siding2_to_main1(
    logic.allocator()->Allocate("condnext_main5_to_1"));

std::unique_ptr<GlobalVariable> condnext_main3_to_2(
    logic.allocator()->Allocate("condnext_main3_to_2"));
std::unique_ptr<GlobalVariable> condnext_stub1_to_main2(
    logic.allocator()->Allocate("condnext_stub1_to_main2"));

std::unique_ptr<GlobalVariable> condnext_unused1(
    logic.allocator()->Allocate("condnext_unused1"));
std::unique_ptr<GlobalVariable> condnext_unused2(
    logic.allocator()->Allocate("condnext_unused2"));
std::unique_ptr<GlobalVariable> condnext_unused3(
    logic.allocator()->Allocate("condnext_unused3"));
std::unique_ptr<GlobalVariable> condnext_unused4(
    logic.allocator()->Allocate("condnext_unused4"));
std::unique_ptr<GlobalVariable> condnext_unused5(
    logic.allocator()->Allocate("condnext_unused5"));
std::unique_ptr<GlobalVariable> condnext_unused6(
    logic.allocator()->Allocate("condnext_unused6"));

void TurnoutAwareABS(Automata* aut, const LocalVariable& turnout,
                     bool expected_direction, const LocalVariable& nextblock,
                     const LocalVariable& secondblock, LocalVariable* red,
                     LocalVariable* green, LocalVariable* condnext) {
  Def()
      .IfReg(turnout, !expected_direction)
      .ActReg1(condnext)
      .ActReg1(red)
      .ActReg0(green);
  Def()
      .IfReg(turnout, expected_direction)
      .IfReg1(nextblock)
      .ActReg1(condnext)
      .ActReg1(red)
      .ActReg0(green);
  Def()
      .IfReg(turnout, expected_direction)
      .IfReg0(nextblock)
      .IfReg1(secondblock)
      .ActReg0(condnext)
      .ActReg1(red)
      .ActReg1(green);
  Def()
      .IfReg(turnout, expected_direction)
      .IfReg0(nextblock)
      .IfReg0(secondblock)
      .ActReg0(condnext)
      .ActReg0(red)
      .ActReg1(green);
}

void ABS(Automata* aut, const LocalVariable& nextblock,
         const LocalVariable& secondblock, LocalVariable* red,
         LocalVariable* green) {
  Def().IfReg1(nextblock).ActReg1(red).ActReg0(green);
  Def().IfReg0(nextblock).IfReg1(secondblock).ActReg1(red).ActReg1(green);
  Def().IfReg0(nextblock).IfReg0(secondblock).ActReg0(red).ActReg1(green);
}

DefAut(sig_32, brd, {
  Def().ActReg0(ImportVariable(constant_false.get()));
  const auto& turnout = ImportVariable(turnout_stub1);
  const auto& nextblock = ImportVariable(*agg_main12);
  const auto& secondblock = ImportVariable(*next_main1);

  auto* red = ImportVariable(&signal_main3_red);
  auto* green = ImportVariable(&signal_main3_green);
  TurnoutAwareABS(aut, turnout, false, nextblock, secondblock, red, green,
                  ImportVariable(condnext_main3_to_2.get()));
});

DefAut(sig_to_main4, brd, {
  const auto& turnout = ImportVariable(turnout_siding1);
  const auto& nextblock = ImportVariable(*agg_main34);
  const auto& secondblock = ImportVariable(*condnext_main3_to_2);
  {
    auto* red = ImportVariable(&signal_main5_to_4_red);
    auto* green = ImportVariable(&signal_main5_to_4_green);
    TurnoutAwareABS(aut, turnout, true, nextblock, secondblock, red, green,
                    ImportVariable(condnext_main5_to_4.get()));
  }
  {
    auto* red = ImportVariable(&signal_siding1_red);
    auto* green = ImportVariable(&signal_siding1_green);
    TurnoutAwareABS(aut, turnout, false, nextblock, secondblock, red, green,
                    ImportVariable(condnext_siding1_to_main4.get()));
  }
});

DefAut(sig_to_main1, brd, {
  const auto& turnout = ImportVariable(turnout_siding2);
  const auto& nextblock = ImportVariable(*agg_main12);
  const auto& secondblock = ImportVariable(*next_main2);
  {
    auto* red = ImportVariable(&signal_main5_to_1_red);
    auto* green = ImportVariable(&signal_main5_to_1_green);
    TurnoutAwareABS(aut, turnout, true, nextblock, secondblock, red, green,
                    ImportVariable(condnext_main5_to_1.get()));
  }
  {
    auto* red = ImportVariable(&signal_siding2_red);
    auto* green = ImportVariable(&signal_siding2_green);
    TurnoutAwareABS(aut, turnout, false, nextblock, secondblock, red, green,
                    ImportVariable(condnext_siding2_to_main1.get()));
  }
});

// Copies the block occupancy from the turnout's currently set direction to a
// virtual "next" block.
void CopyTurnout(Automata* aut, const LocalVariable& turnout,
                 const LocalVariable& block_closed,
                 const LocalVariable& block_thrown,
                 LocalVariable* vblock_next) {
  Def().IfReg0(turnout).IfReg0(block_closed).ActReg0(vblock_next);
  Def().IfReg0(turnout).IfReg1(block_closed).ActReg1(vblock_next);
  Def().IfReg1(turnout).IfReg0(block_thrown).ActReg0(vblock_next);
  Def().IfReg1(turnout).IfReg1(block_thrown).ActReg1(vblock_next);
}

DefAut(nextblocks, brd, {
  CopyTurnout(aut, ImportVariable(turnout_stub1), ImportVariable(block_main3),
              ImportVariable(block_stub1), ImportVariable(next_main2.get()));
  CopyTurnout(aut, ImportVariable(turnout_stub2), ImportVariable(block_stub2),
              ImportVariable(block_siding1),
              ImportVariable(next_siding2.get()));
  // todo: we also need to copy next-next bits from the turnouts beyond siding1.

  // todo: what happens if the user wants to go from main4 to siding1 but
  // turnout is set towards the factory ? it should be red.
  {
    const auto& turnout = ImportVariable(turnout_siding1);
    auto* vblock_next = ImportVariable(next_main4.get());
    const auto& siding1 = ImportVariable(block_siding1);
    const auto& siding2 = ImportVariable(block_siding2);
    const auto& turnout_s = ImportVariable(turnout_stub2);
    const auto& main5 = ImportVariable(block_main5);
    Def().IfReg1(turnout).IfReg0(main5).ActReg0(vblock_next);
    Def().IfReg1(turnout).IfReg1(main5).ActReg1(vblock_next);
    Def().IfReg0(turnout).IfReg1(siding1).ActReg1(vblock_next);
    Def().IfReg0(turnout).IfReg1(siding2).ActReg1(vblock_next);
    Def().IfReg0(turnout).IfReg0(turnout_s).ActReg1(vblock_next);
    Def()
        .IfReg0(turnout)
        .IfReg1(turnout_s)
        .IfReg0(siding2)
        .IfReg0(siding1)
        .ActReg0(vblock_next);
  }

  // The siding is free if the turnout is thrown and both siding1 and siding2
  // is free, or if the turnout is thrown and both siding2 and stub2 is free.
  OccupancyAggregate(aut, ImportVariable(block_siding2),
                     ImportVariable(*next_siding2),
                     ImportVariable(agg_siding2_from_main1.get()));
  CopyTurnout(aut, ImportVariable(turnout_siding2),
              ImportVariable(*agg_siding2_from_main1),
              ImportVariable(block_main5), ImportVariable(next_main1.get()));
});

DefAut(sig_23, brd, {
  const auto& turnout = ImportVariable(turnout_stub1);
  {
    const auto& nextblock = ImportVariable(*agg_main34);
    const auto& secondblock = ImportVariable(*next_main4);

    auto* red = ImportVariable(&signal_main2_top_red);
    auto* green = ImportVariable(&signal_main2_top_green);
    TurnoutAwareABS(aut, turnout, false, nextblock, secondblock, red, green,
                    ImportVariable(condnext_unused1.get()));
  }
  {
    const auto& nextblock = ImportVariable(block_stub1);
    const auto& secondblock = ImportVariable(*constant_false);
    auto* red = ImportVariable(&signal_main2_bot_red);
    auto* green = ImportVariable(&signal_main2_bot_green);
    TurnoutAwareABS(aut, turnout, true, nextblock, secondblock, red, green,
                    ImportVariable(condnext_unused2.get()));
  }
});

DefAut(sig_1, brd, {
  const auto& turnout = ImportVariable(turnout_siding2);
  const auto& nextblock = ImportVariable(*next_main1);
  {
    // todo: figure out what the next would be from siding2 to siding1
    const auto& secondblock = ImportVariable(*constant_false);
    auto* red = ImportVariable(&signal_main1_top_red);
    auto* green = ImportVariable(&signal_main1_top_green);
    TurnoutAwareABS(aut, turnout, false, nextblock, secondblock, red, green,
                    ImportVariable(condnext_unused3.get()));
  }
  {
    const auto& secondblock = ImportVariable(*condnext_main5_to_4);
    auto* red = ImportVariable(&signal_main1_bot_red);
    auto* green = ImportVariable(&signal_main1_bot_green);
    TurnoutAwareABS(aut, turnout, true, nextblock, secondblock, red, green,
                    ImportVariable(condnext_unused4.get()));
  }
});

DefAut(sig_4, brd, {
  const auto& turnout = ImportVariable(turnout_siding1);
  const auto& nextblock = ImportVariable(*next_main4);
  {
    const auto& secondblock = ImportVariable(*condnext_siding2_to_main1);
    auto* red = ImportVariable(&signal_main4_top_red);
    auto* green = ImportVariable(&signal_main4_top_green);
    TurnoutAwareABS(aut, turnout, false, nextblock, secondblock, red, green,
                    ImportVariable(condnext_unused5.get()));
  }
  {
    const auto& secondblock = ImportVariable(*condnext_main5_to_1);
    auto* red = ImportVariable(&signal_main4_bot_red);
    auto* green = ImportVariable(&signal_main4_bot_green);
    TurnoutAwareABS(aut, turnout, true, nextblock, secondblock, red, green,
                    ImportVariable(condnext_unused6.get()));
  }
});

DefAut(second_reset_aut, brd, {
  Def().IfReg1(ImportVariable(reset_sw)).ActState(StInit);

  for (auto* var :
       {&block_main1, &block_main2, &block_main3, &block_main4, &block_main5,
        &block_siding1, &block_siding2, &block_stub1, &block_stub2}) {
    auto* lv = ImportVariable(var);
    Def().IfState(StInit).ActReg1(lv).ActReg0(lv);
  }

  ClearUsedVariables();
  for (auto* var :
       {&signal_main3_red, &signal_main3_green, &signal_main2_top_red,
        &signal_main2_top_green, &signal_main2_bot_red, &signal_main2_bot_green,
        &signal_main5_to_4_red, &signal_main5_to_4_green, &signal_siding1_red,
        &signal_siding1_green, &signal_main5_to_1_red, &signal_main5_to_1_green,
        &signal_siding2_red, &signal_siding2_green, &signal_main1_top_red,
        &signal_main1_top_green, &signal_main1_bot_red, &signal_main1_bot_green,
        &signal_main4_top_red, &signal_main4_top_green, &signal_main4_bot_red,
        &signal_main4_bot_green}) {
    auto* lv = ImportVariable(var);
    Def().IfState(StInit).ActReg1(lv).ActReg0(lv);
  }

  Def().IfState(StInit).ActReg0(ImportVariable(&reset_sw)).ActState(StUser1);

  ClearUsedVariables();

  StateRef Stt1a(10);
  StateRef Stt1b(11);
  StateRef Stt2a(12);
  StateRef Stt2b(13);
  StateRef Stt3a(14);
  StateRef Stt3b(15);
  StateRef Stt4a(16);
  StateRef Stt4b(17);

  Def().IfState(StUser1).ActState(Stt1a);

#define SWITCHSTATE(prev_state, DirAct, turnout, nextstate) \
  Def()                                                     \
      .IfState(prev_state)                                  \
      .IfTimerDone()                                        \
      .DirAct(ImportVariable(&turnout))                     \
      .ActTimer(2)                                          \
      .ActState(nextstate)

  SWITCHSTATE(Stt1a, ActReg0, turnout_siding1, Stt1b);
  SWITCHSTATE(Stt1b, ActReg1, turnout_siding1, Stt2a);
  SWITCHSTATE(Stt2a, ActReg0, turnout_siding2, Stt2b);
  SWITCHSTATE(Stt2b, ActReg1, turnout_siding2, Stt3a);
  SWITCHSTATE(Stt3a, ActReg1, turnout_stub1, Stt3b);
  SWITCHSTATE(Stt3b, ActReg0, turnout_stub1, Stt4a);
  SWITCHSTATE(Stt4a, ActReg0, turnout_stub2, Stt4b);
  SWITCHSTATE(Stt4b, ActReg1, turnout_stub2, StBase);
});

unique_ptr<GlobalVariable> blink_variable(NewTempVariable(&brd));

EventBasedVariable led(&brd, "led", 0x0502010202650012ULL,
                       0x0502010202650013ULL, 7, 31, 1);

DefAut(blinkaut, brd, {
  const int kBlinkSpeed = 20;
  LocalVariable* rep(ImportVariable(blink_variable.get()));
  Def().IfState(StInit).ActState(StUser1);
  Def()
      .IfState(StUser1)
      .IfTimerDone()
      .ActTimer(kBlinkSpeed)
      .ActState(StUser2)
      .ActReg0(rep);
  Def()
      .IfState(StUser2)
      .IfTimerDone()
      .ActTimer(kBlinkSpeed)
      .ActState(StUser1)
      .ActReg1(rep);

  // DefCopy(*rep, ImportVariable(&bd.LedGoldSw));
});

static const int kNextBlockTime = 3;
static const int kTransitionTime = 1;

unique_ptr<GlobalVariable> trans_cw(NewTempVariable(&brd));
unique_ptr<GlobalVariable> trans_ccw(NewTempVariable(&brd));
unique_ptr<GlobalVariable> trans_closed(NewTempVariable(&brd));
unique_ptr<GlobalVariable> trans_thrown(NewTempVariable(&brd));

void AddTransition(Automata* aut, const LocalVariable& go_var,
                   LocalVariable* trans_var, const StateRef& StPrev,
                   const StateRef& StNext, GlobalVariable* block_prev,
                   GlobalVariable* block_next) {
  Def()
      .IfState(StPrev)
      .IfReg1(go_var)
      .IfReg0(*trans_var)
      .IfTimerDone()
      .ActReg1(trans_var)
      .ActTimer(kTransitionTime)
      .ActReg1(aut->ImportVariable(block_next));

  Def()
      .IfState(StPrev)
      .IfReg1(*trans_var)
      .IfTimerDone()
      .ActState(StNext)
      .ActTimer(kNextBlockTime)
      .ActReg0(trans_var)
      .ActReg0(aut->ImportVariable(block_prev));
}

void AddCondTransition(Automata* aut, const LocalVariable& go_var,
                       LocalVariable* trans_var, const StateRef& StPrev,
                       const StateRef& StNext, GlobalVariable* block_prev,
                       GlobalVariable* block_next,
                       const LocalVariable& condition, bool value) {
  Def()
      .IfState(StPrev)
      .IfReg1(go_var)
      .IfReg0(*trans_var)
      .IfReg(condition, value)
      .IfTimerDone()
      .ActReg1(trans_var)
      .ActTimer(kTransitionTime)
      .ActReg1(aut->ImportVariable(block_next));

  Def()
      .IfState(StPrev)
      .IfReg1(*trans_var)
      .IfTimerDone()
      .ActState(StNext)
      .ActReg0(trans_var)
      .ActTimer(kNextBlockTime)
      .ActReg0(aut->ImportVariable(block_prev));
}

// link straight blocks: A to B is clockwise
void LinkBlock(Automata* aut, const StateRef& StA, const StateRef& StB,
               GlobalVariable* block_A, GlobalVariable* block_B) {
  AddTransition(aut, aut->ImportVariable(go_cw),
                aut->ImportVariable(trans_cw.get()), StA, StB, block_A,
                block_B);
  AddTransition(aut, aut->ImportVariable(go_ccw),
                aut->ImportVariable(trans_ccw.get()), StB, StA, block_B,
                block_A);
}

// link straight blocks: A to B is clockwise
void LinkTurnout(Automata* aut, const StateRef& StP, const StateRef& StC,
                 const StateRef& StT, GlobalVariable* block_P,
                 GlobalVariable* block_C, GlobalVariable* block_T,
                 const LocalVariable& go_PT, const LocalVariable& go_TP,
                 LocalVariable* trans_TP, const LocalVariable& turnout) {
  AddTransition(aut, go_TP, trans_TP, StT, StP, block_T, block_P);
  AddTransition(aut, go_TP, trans_TP, StC, StP, block_C, block_P);

  AddCondTransition(aut, go_PT, aut->ImportVariable(trans_closed.get()), StP,
                    StC, block_P, block_C, turnout, false);
  AddCondTransition(aut, go_PT, aut->ImportVariable(trans_thrown.get()), StP,
                    StT, block_P, block_T, turnout, true);
}

DefAut(vtrainaut, brd, {
  ResetUserState(StInit.state + 1);
  static StateRef StGoOff(NewUserState());
  static StateRef StOff(NewUserState());
  static StateRef StCreate(NewUserState());

  auto* lgo_cw = ImportVariable(&go_cw);
  auto* lgo_ccw = ImportVariable(&go_ccw);
  auto* lgo_stop = ImportVariable(&go_stop);
  auto* lgo_off = ImportVariable(&go_off);

  auto* lreq_cw = ImportVariable(&req_cw);
  auto* lreq_ccw = ImportVariable(&req_ccw);
  auto* lreq_stop = ImportVariable(&req_stop);
  auto* lreq_off = ImportVariable(&req_off);

  static StateRef St5(NewUserState());
  static StateRef St4(NewUserState());
  static StateRef St3(NewUserState());
  static StateRef St2(NewUserState());
  static StateRef St1(NewUserState());
  static StateRef StS1(NewUserState());
  static StateRef StS2(NewUserState());
  static StateRef StI1(NewUserState());
  static StateRef StI2(NewUserState());

  Def().IfState(StInit).ActState(StGoOff);

  Def().IfReg1(*lreq_off).IfReg0(*lgo_off).ActState(StGoOff);
  Def()
      .IfState(StGoOff)
      .ActReg0(lreq_cw)
      .ActReg0(lgo_cw)
      .ActReg0(lreq_ccw)
      .ActReg0(lgo_ccw)
      .ActReg0(lreq_stop)
      .ActReg0(lgo_stop)
      .ActReg1(lreq_off)
      .ActReg1(lgo_off);

  for (auto* var :
       {&block_main1, &block_main2, &block_main3, &block_main4, &block_main5,
        &block_siding1, &block_siding2, &block_stub1, &block_stub2}) {
    auto* lv = ImportVariable(var);
    Def().IfState(StGoOff).ActReg0(lv);
  }

  Def().IfState(StGoOff).ActState(StOff);
  Def().IfState(StOff).IfReg1(*lreq_cw).ActState(StCreate);
  Def().IfState(StOff).IfReg1(*lreq_ccw).ActState(StCreate);
  Def().IfState(StOff).IfReg1(*lreq_stop).ActState(StCreate);

  Def().IfState(StCreate).ActReg1(ImportVariable(&block_main5)).ActReg0(lreq_off).ActReg0(lgo_off).ActState(St5);

  // Radio-button behavior. If a request comes in, we remove all other shadow
  // variables and ack the request by copying it to the shadow variable.

  Def()
      .IfReg1(*lreq_cw)
      .IfReg0(*lgo_cw)
      .ActReg1(lgo_cw)
      .ActReg0(lgo_ccw)
      .ActReg0(lgo_stop)
      .ActReg0(lgo_off)
      .ActReg0(lreq_ccw)
      .ActReg0(lreq_stop)
      .ActReg0(lreq_off)
      .ActTimer(kTransitionTime);

  Def()
      .IfReg1(*lreq_ccw)
      .IfReg0(*lgo_ccw)
      .ActReg1(lgo_ccw)
      .ActReg0(lgo_cw)
      .ActReg0(lgo_stop)
      .ActReg0(lgo_off)
      .ActReg0(lreq_cw)
      .ActReg0(lreq_stop)
      .ActReg0(lreq_off)
      .ActTimer(kTransitionTime);

  Def()
      .IfReg1(*lreq_stop)
      .IfReg0(*lgo_stop)
      .ActReg0(lgo_ccw)
      .ActReg0(lgo_cw)
      .ActReg1(lgo_stop)
      .ActReg0(lgo_off)
      .ActReg0(lreq_ccw)
      .ActReg0(lreq_cw)
      .ActReg0(lreq_off);

  Def()
      .IfReg1(*lreq_off)
      .IfReg0(*lgo_off)
      .ActReg0(lgo_ccw)
      .ActReg0(lgo_cw)
      .ActReg0(lgo_stop)
      .ActReg1(lgo_off)
      .ActReg0(lreq_ccw)
      .ActReg0(lreq_cw)
      .ActReg0(lreq_stop)
      .ActState(StGoOff);

  // If a shadow variable was killed, kill the request variable too.

  Def().IfReg0(*lgo_cw).ActReg0(lreq_cw);
  Def().IfReg0(*lgo_ccw).ActReg0(lreq_ccw);
  Def().IfReg0(*lgo_stop).ActReg0(lreq_stop);
  Def().IfReg0(*lgo_off).ActReg0(lreq_off);

  LinkBlock(aut, St4, St3, &block_main4, &block_main3);
  LinkBlock(aut, St2, St1, &block_main2, &block_main1);

  LinkTurnout(aut, StS2, StI2, StS1, &block_siding2, &block_stub2,
              &block_siding1, *lgo_cw, *lgo_ccw,
              ImportVariable(trans_ccw.get()), ImportVariable(turnout_stub2));

  LinkTurnout(aut, St1, StS2, St5, &block_main1, &block_siding2, &block_main5,
              *lgo_cw, *lgo_ccw, ImportVariable(trans_ccw.get()),
              ImportVariable(turnout_siding2));

  LinkTurnout(aut, St4, StS1, St5, &block_main4, &block_siding1, &block_main5,
              *lgo_ccw, *lgo_cw, ImportVariable(trans_cw.get()),
              ImportVariable(turnout_siding1));

  LinkTurnout(aut, St2, St3, StI1, &block_main2, &block_main3, &block_stub1,
              *lgo_ccw, *lgo_cw, ImportVariable(trans_cw.get()),
              ImportVariable(turnout_stub1));
});

int main(int argc, char** argv) {
  FILE* f = fopen("automata.bin", "wb");
  assert(f);
  string output;
  brd.Render(&output);
  fwrite(output.data(), 1, output.size(), f);
  fclose(f);

  //f = fopen("convention-logic.cout", "wb");
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
  fclose(f);

  /*if (automata::GetOffsetMap()) {
    f = fopen("variables.txt", "w");
    assert(f);
    map<int, string>& m(*automata::GetOffsetMap());
    for (const auto& it : m) {
      fprintf(f, "%04x: %s\n", it.first * 2, it.second.c_str());
    }
    fclose(f);
    }*/

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
