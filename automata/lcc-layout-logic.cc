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

EventBasedVariable block_main1(&brd, "main1", INPUT_BASE | 0x00,
                               INPUT_BASE | 01, 0, OFS_IOA, 0);
EventBasedVariable block_main2(&brd, "main2", INPUT_BASE | 0x1E,
                               INPUT_BASE | 0x1E + 1, 0, OFS_IOA, 1);
EventBasedVariable block_main3(&brd, "main3", INPUT_BASE | 0x02,
                               INPUT_BASE | 0x02 + 1, 0, OFS_IOA, 2);
EventBasedVariable block_main4(&brd, "main4", INPUT_BASE | 0x0A,
                               INPUT_BASE | 0x0A + 1, 0, OFS_IOA, 3);
EventBasedVariable block_main5(&brd, "main5", INPUT_BASE | 0x06,
                               INPUT_BASE | 0x06 + 1, 0, OFS_IOA, 4);
EventBasedVariable block_siding1(&brd, "siding1", INPUT_BASE | 0x04,
                                 INPUT_BASE | 0x04 + 1, 0, OFS_IOA, 5);
EventBasedVariable block_siding2(&brd, "siding2", INPUT_BASE | 0x08,
                                 INPUT_BASE | 0x08 + 1, 0, OFS_IOA, 6);

EventBasedVariable block_stub1(&brd, "stub1", INPUT_BASE | 0x18,
                               INPUT_BASE | 0x18 + 1, 0, OFS_IOA + 1, 0);
EventBasedVariable block_stub2(&brd, "stub2", INPUT_BASE | 0x1C,
                               INPUT_BASE | 0x1C + 1, 0, OFS_IOA + 1, 1);

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
                           0, OFS_IOB, 0);
EventBasedVariable out_x1b(&brd, "rrx1_b", OUTPUT_BASE | 0x2, OUTPUT_BASE | 0x3,
                           0, OFS_IOB, 1);

std::unique_ptr<GlobalVariable> rrx2(logic.allocator()->Allocate("rr_x2"));
EventBasedVariable out_x2a(&brd, "rrx2_a", OUTPUT_BASE | 0x4, OUTPUT_BASE | 0x5,
                           0, OFS_IOB, 2);
EventBasedVariable out_x2b(&brd, "rrx2_b", OUTPUT_BASE | 0x6, OUTPUT_BASE | 0x7,
                           0, OFS_IOB, 3);

void CrossingBlink(const LocalVariable& in_x, LocalVariable* out_xa,
                   LocalVariable* out_xb) {
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
  Def().IfReg0(in_x).ActState(StBase).ActReg0(out_xa).ActReg0(out_xb);
}

DefAut(crossing1, brd, {
  Def().IfState(StInit).ActState(StBase);
  auto* out_xa = ImportVariable(out_x1a.get());
  auto* out_xb = ImportVariable(out_x1b.get());
  auto* in_x = ImportVariable(*rrx1);
  CrossingBlink(*in_x, out_xa, out_xb);

  Def()
      .IfReg0(ImportVariable(*block_main2))
      .IfReg0(ImportVariable(*block_main3))
      .ActReg0(in_x);
  Def().IfReg1(ImportVariable(*block_main2)).ActReg1(in_x);
  Def().IfReg1(ImportVariable(*block_main3)).ActReg1(in_x);
});

DefAut(crossing2, brd, {
  Def().IfState(StInit).ActState(StBase);
  auto* out_xa = ImportVariable(out_x2a.get());
  auto* out_xb = ImportVariable(out_x2b.get());
  auto* in_x = ImportVariable(*rrx2);
  CrossingBlink(*in_x, out_xa, out_xb);

  Def()
      .IfReg0(ImportVariable(*block_main4))
      .IfReg0(ImportVariable(*block_main5))
      .IfReg0(ImportVariable(*block_siding1))
      .ActReg0(in_x);
  Def().IfReg1(ImportVariable(*block_main4)).ActReg1(in_x);
  Def().IfReg1(ImportVariable(*block_main5)).ActReg1(in_x);
  Def().IfReg1(ImportVariable(*block_siding1)).ActReg1(in_x);
});




unique_ptr<GlobalVariable> blink_variable(NewTempVariable(&brd));

EventBasedVariable led(&brd, "led", 0x0502010202650012ULL,
                       0x0502010202650013ULL, 7, 31, 1);

DefAut(watchdog, brd, {
  LocalVariable* w = ImportVariable(&watchdog);
  Def().IfTimerDone().ActReg1(w).ActReg0(w).ActTimer(1);
});

DefAut(blinkaut, brd, {
  const int kBlinkSpeed = 3;
  LocalVariable* rep(ImportVariable(&bd.LedGoldSw));
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

  // DefCopy(*rep, ImportVariable(&bd.LedGoldSw));
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
