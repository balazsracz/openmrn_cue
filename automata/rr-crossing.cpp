#include <stdio.h>
#include <unistd.h>

#include <string>
#include <fstream>
using namespace std;

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"


#include "../cs/src/base.h"
//#define OFS_GLOBAL_BITS 24

using namespace automata;


#define BRACZ_LAYOUT 0x0501010114FF0000ULL


StateRef StateInit(0);
StateRef StateBase(1);

StateRef StateUser1(2);
StateRef StateUser2(3);


Board brd;

EventBasedVariable led(&brd,
                       BRACZ_LAYOUT | 0x2050,
                       BRACZ_LAYOUT | 0x2051,
                       0, OFS_GLOBAL_BITS, 1);

// Negated input_pin1 variable.
EventBasedVariable vNinput_pin(&brd,
                               0x0502010202650301ULL,
                               0x0502010202650300ULL,
                               0, OFS_GLOBAL_BITS, 2);

EventBasedVariable intev(&brd,
                         0x0502010202650122ULL,
                         0x0502010202650123ULL,
                         0, OFS_GLOBAL_BITS, 3);

EventBasedVariable repev(&brd,
                         0x0502010202650132ULL,
                         0x0502010202650133ULL,
                         0, OFS_GLOBAL_BITS, 4);

EventBasedVariable vled1(&brd,
                         BRACZ_LAYOUT | 0x2050,
                         BRACZ_LAYOUT | 0x2051,
                         1, OFS_IOA, 1);
EventBasedVariable vled2(&brd,
                         BRACZ_LAYOUT | 0x2052,
                         BRACZ_LAYOUT | 0x2053,
                         1, OFS_IOA, 0);
EventBasedVariable vled3(&brd,
                         BRACZ_LAYOUT | 0x2054,
                         BRACZ_LAYOUT | 0x2055,
                         2, OFS_IOA, 1);
EventBasedVariable vled4(&brd,
                         BRACZ_LAYOUT | 0x2056,
                         BRACZ_LAYOUT | 0x2057,
                         2, OFS_IOA, 0);

EventBasedVariable inpb1(&brd,
                         BRACZ_LAYOUT | 0x2060,
                         BRACZ_LAYOUT | 0x2061,
                         1, OFS_IOB, 2);

EventBasedVariable inpb2(&brd,
                         BRACZ_LAYOUT | 0x2070,
                         BRACZ_LAYOUT | 0x2071,
                         2, OFS_IOB, 2);


DefAut(testaut, brd, {
        Def().IfState(StateInit).ActState(StateBase);
    });


DefAut(blinker, brd, {
    LocalVariable* track_busy(ImportVariable(&inpb1));
    LocalVariable* l1(ImportVariable(&vled1));
    LocalVariable* l2(ImportVariable(&vled2));
    Def().IfState(StateInit).ActState(StateBase).ActReg1(track_busy).ActReg0(l1).ActReg0(l2);
    Def().IfReg1(*track_busy).ActState(StateBase);
    Def().IfReg0(*track_busy).IfState(StateBase).ActState(StateUser1);

    Def().IfState(StateUser1).IfTimerDone().ActTimer(1).ActState(StateUser2);
    Def().IfState(StateUser2).IfTimerDone().ActTimer(1).ActState(StateUser1);

    Def().IfState(StateBase).ActReg0(l1);
    Def().IfState(StateBase).ActReg0(l2);

    Def().IfState(StateUser1).ActReg1(l1);
    Def().IfState(StateUser2).ActReg0(l1);

    Def().IfState(StateUser2).ActReg1(l2);
    Def().IfState(StateUser1).ActReg0(l2);
  });

DefAut(blinker2, brd, {
    LocalVariable* track_busy(ImportVariable(&inpb2));
    LocalVariable* l1(ImportVariable(&vled3));
    LocalVariable* l2(ImportVariable(&vled4));
    Def().IfState(StateInit).ActState(StateBase).ActReg1(track_busy).ActReg0(l1).ActReg0(l2);
    Def().IfReg1(*track_busy).ActState(StateBase);
    Def().IfReg0(*track_busy).IfState(StateBase).ActState(StateUser1);

    Def().IfState(StateUser1).IfTimerDone().ActTimer(1).ActState(StateUser2);
    Def().IfState(StateUser2).IfTimerDone().ActTimer(1).ActState(StateUser1);

    Def().IfState(StateBase).ActReg0(l1);
    Def().IfState(StateBase).ActReg0(l2);

    Def().IfState(StateUser1).ActReg1(l1);
    Def().IfState(StateUser2).ActReg0(l1);

    Def().IfState(StateUser2).ActReg1(l2);
    Def().IfState(StateUser1).ActReg0(l2);
  });


/*DefAut(copier, brd, {
    DefCopy(ImportVariable(&vNinput_pin),
            ImportVariable(&led));
    });

DefAut(xcopier, brd, {
        LocalVariable& ledvar(ImportVariable(&vled4));
        LocalVariable& btnvar(ImportVariable(&inpb1));
        //DefCopy(btnvar, ledvar);
    });

DefAut(xcopier2, brd, {
        LocalVariable& ledvar(ImportVariable(&vled2));
        LocalVariable& btnvar(ImportVariable(&inpb2));
        //DefNCopy(btnvar, ledvar);
    });

*/


int main(int argc, char** argv) {
    string output;
    brd.Render(&output);
    printf("const char automata_code[] = {\n  ");
    int c = 0;
    for (char t : output) {
      printf("0x%02x, ", (uint8_t)t);
      if (++c >= 12) {
        printf("\n  ");
        c = 0;
      }
    }
    printf("};\n");
    return 0;
};
