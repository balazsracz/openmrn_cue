#include <stdio.h>
#include <unistd.h>

#include <string>
#include <fstream>
using namespace std;

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"

#define OFS_GLOBAL_BITS 24

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


DefAut(testaut, brd, {
        Def().IfState(StateInit).ActState(StateBase);
    });

DefAut(copier, brd, {
        LocalVariable& ledvar(ImportVariable(&led));
        LocalVariable& intvar(ImportVariable(&intev));
        Def().IfReg1(ledvar).ActReg1(intvar);
        Def().IfReg0(ledvar).ActReg0(intvar);
    });

DefAut(blinker, brd, {
        LocalVariable& repvar(ImportVariable(&repev));
        Def().IfState(StateInit).ActState(StateUser1);
        Def().IfState(StateUser1).IfTimerDone().ActTimer(1).ActState(StateUser2);
        Def().IfState(StateUser2).IfTimerDone().ActTimer(1).ActState(StateUser1);
        Def().IfState(StateUser1).ActReg1(repvar);
        Def().IfState(StateUser2).ActReg0(repvar);
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
