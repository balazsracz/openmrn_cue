#include <assert.h>
#include <stdarg.h>

#include "system.hxx"
#include "../cs/src/automata_defs.h"
#include "operations.hxx"
#include "variables.hxx"


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
    // Render all automata bodies to create temporary variables
    for (auto& a: automatas_) {
      assert(a.automata);
      a.automata->Render(NULL);
    }
    for (auto v: global_variables_) {
        v->SetId(output->size());
        v->Render(output);
    }
    fprintf(stderr, "%d global variables, total size: %d\n", global_variables_.size(), output->size());
    // Terminate the global variable list.
    output->push_back(0);
}

void Board::RenderAutomatas(string* output) {
    for (auto& a: automatas_) {
        a.offset = output->size();
        assert(a.automata);
        a.automata->Render(output);
        // Put back the pointer into the table. Little-endian.
        (*output)[a.ptr_offset] = a.offset & 0xff;
        (*output)[a.ptr_offset + 1] = (a.offset >> 8) & 0xff;
    }
    fprintf(stderr, "%d automtas, total output size: %d\n", automatas_.size(), output->size());
}

Automata::LocalVariable* Automata::ImportVariable(GlobalVariable* var) {
    ImportVariable(*var);
    LocalVariable& ret = used_variables_[var];
    HASSERT(ret.id >= 0);
    return &ret;
}

const Automata::LocalVariable& Automata::ImportVariable(
    const GlobalVariable& var) {
  int next_id = used_variables_.size();
  LocalVariable& ret = used_variables_[&var];
  if (ret.id < 0) {
    ret.id = next_id;
    assert(ret.id < MAX_IMPORT_VAR);
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
            uint8_t b1 = 0;
            uint8_t b2 = 0;
            uint16_t arg = output ? it.first->GetId().arg : 0;
            HASSERT(arg < (8<<8));
            b1 = (arg >> 8) << 5;
            HASSERT(it.second.id < 32);
            b1 |= (it.second.id & 31);
            b2 = arg & 0xff;
            op.push_back(b1);
            op.push_back(b2); // argument, low bits
            int gofs = output ? it.first->GetId().id : 0;
            op.push_back(gofs & 0xff);
            op.push_back((gofs >> 8) & 0xff);
            Op::CreateOperation(output, empty, op);
        } else {
            // Checks that the timer bit is the NULL.
            assert(it.second.id == 0);
        }
    }
    // Actually renders the body.
    Body();
    if (output_) {
      output->push_back(0);  // EOF byte for the runner.
    }
    output_ = NULL;
}

void Automata::DefCopy(const Automata::LocalVariable& src,
                       Automata::LocalVariable* dst) {
  Def().IfReg0(src).IfReg1(*dst).ActReg0(dst);
  Def().IfReg1(src).IfReg0(*dst).ActReg1(dst);
}

void Automata::DefNCopy(const Automata::LocalVariable& src,
                        Automata::LocalVariable* dst) {
  Def().IfReg0(src).IfReg0(*dst).ActReg1(dst);
  Def().IfReg1(src).IfReg1(*dst).ActReg0(dst);
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

typedef map<int, string> OfsMap;
OfsMap* g_ofs_map = nullptr;

GlobalVariable* EventBlock::Allocator::Allocate(const string& name) const {
  int ofs = next_entry_;
  BlockVariable* v = new BlockVariable(this, name);
  if (!g_ofs_map) {
    g_ofs_map = new OfsMap;
  }
  (*g_ofs_map)[ofs] = v->name();
  return v;
}

void ClearOffsetMap() {
  if (g_ofs_map) g_ofs_map->clear();
}

map<int, string>* GetOffsetMap() {
  return g_ofs_map;
}

}  // namespace automata

const string& GetNameForOffset(int ofs) {
  static string empty;
  automata::OfsMap::const_iterator it = automata::g_ofs_map->find(ofs);
  if (it != automata::g_ofs_map->end()) {
    return it->second;
  } else {
    return empty;
  }
}

string StringPrintf(const char* format, ...) {
  static const int kBufSize = 1000;
  char buffer[kBufSize];
  va_list ap;

  va_start(ap, format);
  int n = vsnprintf(buffer, kBufSize, format, ap);
  va_end(ap);
  HASSERT(n >= 0);
  if (n < kBufSize) {
    return string(buffer, n);
  }
  string ret(n + 1, 0);
  va_start(ap, format);
  n = vsnprintf(&ret[0], ret.size(), format, ap);
  va_end(ap);
  HASSERT(n >= 0);
  ret.resize(n);
  return ret;
}
