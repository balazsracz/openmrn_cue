#include <assert.h>
#include <stdarg.h>

#include "system.hxx"
#include "../cs/src/automata_defs.h"
#include "operations.hxx"
#include "variables.hxx"


namespace automata {

__attribute__((__weak__)) int FIRST_USER_STATE_ID = 10;

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
    fprintf(stderr, "%d automatas, total output size: %d\n", automatas_.size(), output->size());
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
    RenderImportVariable(var, ret.id);
  }
  return ret;
}

void Automata::RenderImportVariable(const GlobalVariable& var,
                                    int local_id) {
  Def().ActImportVariable(var, local_id);
}

Automata::LocalVariable&& Automata::ReserveVariable() {
  const GlobalVariable* p = (const GlobalVariable*)(-1);
  while (used_variables_.find(p) != used_variables_.end()) {
    --p;
  }
  int id = used_variables_.size();
  used_variables_[p].id = id;
  return LocalVariable(id);
}

void Automata::ClearUsedVariables() {
  used_variables_.clear();
  // We add the timer variable to the map with a fake key in order to
  // reserve local bit 0.
  used_variables_[NULL] = timer_bit_;
}

void Automata::Render(string* output) {
    output_ = NULL;
    // This will allocate all variables without outputing anything.
    //Body();
    output_ = output;
    // Actually renders the body.
    ClearUsedVariables();
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

namespace NMRAnet {
const string& GetNameForOffset(int ofs) {
  static string empty;
  automata::OfsMap::const_iterator it = automata::g_ofs_map->find(ofs);
  if (it != automata::g_ofs_map->end()) {
    return it->second;
  } else {
    return empty;
  }
}
}

namespace automata {
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
}
