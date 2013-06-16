#include "system.hxx"
#include "../cs/src/automata_defs.h"
#include "operations.hxx"


namespace automata {

Board::~Board() {
}

void Board::Render(string* output) {
    RenderPreamble(output);
    RenderAutomatas(output);
}

void Board::RenderPreamble(string* output) {
    for (auto& a: automatas_) {
        a.ptr_offset = output->size();
        // Put a pointer to the output.
        output->push_back(0);
        output->push_back(0);
    }
    // Terminate the automata list.
    output->push_back(0);
    output->push_back(0);
    for (auto v: global_variables_) {
        v->SetId(output->size());
        v->Render(output);
    }
    // Terminate the global variable list.
    output->push_back(0);
}

void Board::RenderAutomatas(string* output) {
    for (auto& a: automatas_) {
        a.offset = output->size();
        a.automata->Render(output);
        // Put back the pointer into the table. Little-endian.
        (*output)[a.ptr_offset] = a.offset & 0xff;
        (*output)[a.ptr_offset + 1] = (a.offset >> 8) & 0xff;
    }
}

Automata::LocalVariable& Automata::ImportVariable(GlobalVariable* var) {
    int next_id = used_variables_.size(); 
    LocalVariable& ret = used_variables_[var];
    if (ret.id < 0) {
        ret.id = next_id;
    }
    return ret;
}

void Automata::Render(string* output) {
    output_ = NULL;
    // This will allocate all variables without outputing anything.
    Body();
    output_ = output;
    // Adds variable imports.
    vector<uint8_t> op;
    vector<uint8_t> empty;
    for (auto& it : used_variables_) {
        if (it.first) {
            op.clear();
            op.push_back(_ACT_IMPORT_VAR);
            op.push_back(it.second.id);
            int gofs = it.first->GetId().id;
            op.push_back(gofs & 0xff);
            op.push_back((gofs >> 8) & 0xff);
            Op::CreateOperation(output, empty, op);
        } else {
            assert(it.second.id == 0);
        }
    }
    // Actually renders the body.
    Body();
    output->push_back(
    output_ = NULL;
}

/*class MyAut : public Automata {

protected:
    virtual void Body(Operations& Def) {
        DefOp()
            .IfState(STATE_BASE)
            .IfTimerNotDone()
            .If0(destvar)
            .ActState(STATE_INIT)
            .Act1(going);

        DefCopy(src, dest);

        DefOp()
            .ActState(STATE_BASE);

    }
    };*/

}
