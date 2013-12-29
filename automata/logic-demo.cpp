#include <stdio.h>
#include <unistd.h>

#include <string>
#include <fstream>
using namespace std;

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"
#include "bracz-layout.hxx"

#include "../cs/src/base.h"
//#define OFS_GLOBAL_BITS 24

using namespace automata;


#define BRACZ_LAYOUT 0x0501010114FF0000ULL


StateRef StateInit(0);
StateRef StateBase(1);

StateRef StateUser1(2);
StateRef StateUser2(3);


Board brd;

I2CBoard ext24(0x24);
I2CSignal sg1(&ext24, 142, "fmain1");
I2CSignal sg2(&ext24, 139, "fmain2");
I2CSignal sg3(&ext24, 138, "adv1");
I2CSignal sg4(&ext24, 144, "adv2");


DefAut(blinker, brd, {
    LocalVariable* l1(ImportVariable(&ext24.LedGreen));
    LocalVariable* l2(ImportVariable(&ext24.RelGreen));
    Def().IfState(StateInit).ActState(StateBase).ActReg0(l1).ActReg0(l2);
    Def().IfState(StateBase).ActState(StateUser1);

    Def().IfState(StateUser1).IfTimerDone().ActTimer(2).ActState(StateUser2);
    Def().IfState(StateUser2).IfTimerDone().ActTimer(2).ActState(StateUser1);

    Def().IfState(StateUser1).ActReg1(l1);
    Def().IfState(StateUser2).ActReg0(l1);

    Def().IfState(StateUser2).ActReg1(l2);
    Def().IfState(StateUser1).ActReg0(l2);

  });


DefAut(allblink, brd, {
    static StateRef st1(NewUserState());
    static StateRef st2(NewUserState());
    static StateRef st3(NewUserState());
    static StateRef st4(NewUserState());
    LocalVariable* lsg1(ImportVariable(&sg1.signal));
    LocalVariable* lsg2(ImportVariable(&sg2.signal));
    LocalVariable* lsg3(ImportVariable(&sg3.signal));
    LocalVariable* lsg4(ImportVariable(&sg4.signal));
    static const int kWaitTime = 1;

    Def().IfState(StateInit).ActState(StateBase).ActSetValue(lsg1, 1, A_STOP);
    
    Def().ActGetValueToAspect(*lsg1, 1);
    Def().IfState(StateBase).IfAspect(A_GO).ActSetAspect(A_STOP).ActState(st1).ActTimer(kWaitTime);
    Def().IfState(StateBase).ActUpAspect();
    Def().IfState(StateBase).ActState(st1).ActTimer(kWaitTime);

    Def().IfState(st1).ActSetValueFromAspect(lsg1, 1);
    Def().IfState(st1).IfTimerDone().ActState(st2).ActTimer(kWaitTime);

    Def().IfState(st2).ActSetValueFromAspect(lsg2, 1);
    Def().IfState(st2).IfTimerDone().ActState(st3).ActTimer(kWaitTime);

    Def().IfState(st3).ActSetValueFromAspect(lsg3, 1);
    Def().IfState(st3).IfTimerDone().ActState(st4).ActTimer(kWaitTime);

    Def().IfState(st4).ActSetValueFromAspect(lsg4, 1);
    Def().IfState(st4).IfTimerDone().ActState(StateBase);

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
