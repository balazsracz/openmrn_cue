#include <stdio.h>
#include <unistd.h>

#include <string>
#include <fstream>
using namespace std;

#include "system.hxx"
#include "operations.hxx"

using namespace automata;


StateRef StateInit(0);
StateRef StateBase(1);

Board brd;

DefAut(testaut, brd, {
        Def().IfState(StateInit).ActState(StateBase);

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
