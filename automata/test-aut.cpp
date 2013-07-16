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


StateRef StateInit(0);
StateRef StateBase(1);

StateRef StateUser1(2);
StateRef StateUser2(3);


Board brd;

EventBasedVariable led(&brd,
                       0x0502010202650012ULL,
                       0x0502010202650013ULL,
                       0, OFS_GLOBAL_BITS, 1);

EventBasedVariable intev(&brd,
                         0x0502010202650022ULL,
                         0x0502010202650023ULL,
                         0, OFS_GLOBAL_BITS, 3);

EventBasedVariable repev(&brd,
                         0x0502010202650032ULL,
                         0x0502010202650033ULL,
                         0, OFS_GLOBAL_BITS, 4);

EventBasedVariable vled1(&brd,
                         0x0502010202650040ULL,
                         0x0502010202650041ULL,
                         1, OFS_IOA, 0);
EventBasedVariable vled2(&brd,
                         0x0502010202650042ULL,
                         0x0502010202650043ULL,
                         1, OFS_IOA, 1);
EventBasedVariable vled3(&brd,
                         0x0502010202650044ULL,
                         0x0502010202650045ULL,
                         2, OFS_IOA, 0);
EventBasedVariable vled4(&brd,
                         0x0502010202650046ULL,
                         0x0502010202650047ULL,
                         2, OFS_IOA, 1);



DefAut(testaut, brd, {
        Def().IfState(StateInit).ActState(StateBase);
    });


DefAut(blinker, brd, {
        LocalVariable& repvar(ImportVariable(&led));
        LocalVariable& l1(ImportVariable(&vled1));
        LocalVariable& l2(ImportVariable(&vled3));
        Def().IfState(StateInit).ActState(StateUser1);
        Def().IfState(StateUser1).IfTimerDone().ActTimer(1).ActState(StateUser2);
        Def().IfState(StateUser2).IfTimerDone().ActTimer(1).ActState(StateUser1);
        Def().IfState(StateUser1).ActReg1(repvar);
        Def().IfState(StateUser2).ActReg0(repvar);

        Def().IfState(StateUser1).ActReg1(l1);
        Def().IfState(StateUser2).ActReg0(l1);

        Def().IfState(StateUser2).ActReg1(l2);
        Def().IfState(StateUser1).ActReg0(l2);

   });

DefAut(copier, brd, {
        LocalVariable& ledvar(ImportVariable(&led));
        LocalVariable& intvar(ImportVariable(&intev));
        Def().IfReg1(ledvar).ActReg1(intvar);
        Def().IfReg0(ledvar).ActReg0(intvar);
    });



int main(int argc, char** argv) {
    FILE* f = fopen("automata.bin", "wb");
    assert(f);
    string output;
    brd.Render(&output);
    fwrite(output.data(), 1, output.size(), f);
    fclose(f);
    return 0;
};
