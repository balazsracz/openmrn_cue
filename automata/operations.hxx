#ifndef _bracz_train_automata_operations_hxx_
#define _bracz_train_automata_operations_hxx_

#include <initializer_list>
#include <math.h>

#include "system.hxx"
#include "../cs/src/automata_defs.h"
#include "callback.hxx"

namespace automata {

#define GET_INVERSE_MASK(x) ( (~(x)) & ((x) - 1) ) 

struct StateRef {
    constexpr StateRef(int id)
        : state(id) {}

    constexpr StateRef(const StateRef& ref)
        : state(ref.state) {}

    const int state;
};

typedef Callback1<Automata::Op*> OpCallback;

/*class OpCallback {
 public:
  virtual void Run(Automata::Op* op) = 0;
  virtual ~OpCallback() {};
};

template<class T, class P1> class OpCallback1 : public OpCallback {
 public:
  typedef void (T::*fptr_t)(P1 p1, Automata::Op* op);

  OpCallback1(T* parent, P1 p1, fptr_t fptr)
      : parent_(parent), p1_(p1), fptr_(fptr) {}
  virtual ~OpCallback1() {}
  virtual void Run(Automata::Op* op) {
    (parent_->*fptr_)(p1_, op);
  }

 private:
  T* parent_;
  P1 p1_;
  fptr_t fptr_;
};

template<class T, class P1> OpCallback1<T, P1> NewCallback(T* obj, typename OpCallback1<T, P1>::fptr_t fptr, P1 p1) {
  return OpCallback1<T, P1>(obj, p1, fptr);
}

template<class T, class P1, class P2> class OpCallback2 : public OpCallback {
 public:
  typedef void (T::*fptr_t)(P1 p1, P2 p2, Automata::Op* op);

  OpCallback2(T* parent, P1 p1, P2 p2, fptr_t fptr)
      : parent_(parent), p1_(p1), p2_(p2), fptr_(fptr) {}
  virtual ~OpCallback2() {}
  virtual void Run(Automata::Op* op) {
    (parent_->*fptr_)(p1_, p2_, op);
  }

 private:
  T* parent_;
  P1 p1_;
  P2 p2_;
  fptr_t fptr_;
};

template<class T, class P1, class P2> OpCallback2<T, P1, P2> NewCallback(T* obj, typename OpCallback2<T, P1, P2>::fptr_t fptr, P1 p1, P2 p2) {
  return OpCallback2<T, P1, P2>(obj, p1, p2, fptr);
  }*/

class Automata::Op {
public:
    Op(Automata* parent)
        : parent_(parent), output_(parent->output_) {}

    Op(Automata* parent, string* output, string f = "unknown", int line = 0)
        : parent_(parent), output_(output), file_(f), line_(line) {}

    ~Op() {
        if (output_) {
          unsigned ofs = output_->size();
          Debug("ofs %u: at %s:%d [%d?->%d]", ofs, file_.c_str(), line_,
                ifs_.size(), acts_.size());
        }
        CreateOperation(output_, ifs_, acts_);
    }

    static void CreateOperation(string* output,
                                const vector<uint8_t>& ifs,
                                const vector<uint8_t>& acts) {
        assert(ifs.size() < 16);
        assert(acts.size() < 16);
        if (!output) return;
        uint8_t hdr = 0;
        hdr = (acts.size() & 0xf) << 4;
        hdr |= ifs.size() & 0xf;
        output->push_back(hdr);
        output->append((char*)ifs.data(), ifs.size());
        output->append((char*)acts.data(), acts.size());
    }

    Op& ActImportVariable(const GlobalVariable& var, const LocalVariable& lv) {
      return ActImportVariable(var, lv.id);
    }

    Op& ActImportVariable(const GlobalVariable& var, int local_id) {
      HASSERT(local_id >= 0);
      acts_.push_back(_ACT_IMPORT_VAR);
      uint8_t b1 = 0;
      uint8_t b2 = 0;
      uint16_t arg = output_ ? var.GetId().arg : 0;
      HASSERT(arg < (8 << 8));
      b1 = (arg >> 8) << 5;
      HASSERT(local_id < 32);
      b1 |= (local_id & 31);
      b2 = arg & 0xff;
      acts_.push_back(b1);
      acts_.push_back(b2);  // argument, low bits
      int gofs = output_ ? var.GetId().id : 0;
      acts_.push_back(gofs & 0xff);
      acts_.push_back((gofs >> 8) & 0xff);
      return *this;
    }

    Op& IfState(const StateRef state) {
      HASSERT((state.state & GET_INVERSE_MASK(_IF_STATE_MASK)) == state.state);
      ifs_.push_back(_IF_STATE | state.state);
      return *this;
    }

    Op& IfReg0(const Automata::LocalVariable& var) {
        uint8_t v = _IF_REG;
        if (output_) v |= var.GetId();
        ifs_.push_back(v);
        return *this;
    }

    Op& IfReg1(const Automata::LocalVariable& var) {
        uint8_t v = _IF_REG | _REG_1;
        if (output_) v |= var.GetId();
        ifs_.push_back(v);
        return *this;
    }

    Op& IfReg(const Automata::LocalVariable& var, bool value) {
        uint8_t v = _IF_REG;
        if (value) v |= _REG_1;
        if (output_) v |= var.GetId();
        ifs_.push_back(v);
        return *this;
    }

    Op& MaybeIfReg(bool enabled, const Automata::LocalVariable& var,
                   bool value) {
        if (!enabled) {
            return *this;
        }
        return IfReg(var, value);
    }

    Op& ActReg0(Automata::LocalVariable* var) {
        uint8_t v = _ACT_REG;
        if (output_) v |= var->GetId();
        acts_.push_back(v);
        return *this;
    }

    Op& ActReg1(Automata::LocalVariable* var) {
        uint8_t v = _ACT_REG | _REG_1;
        if (output_) v |= var->GetId();
        acts_.push_back(v);
        return *this;
    }

    Op& ActReg(Automata::LocalVariable* var, bool value) {
        uint8_t v = _ACT_REG;
        if (value) v |= _REG_1;
        if (output_) v |= var->GetId();
        acts_.push_back(v);
        return *this;
    }

    Op& MaybeActReg(bool enabled, Automata::LocalVariable* var, bool value) {
        if (!enabled) {
            return *this;
        }
        return ActReg(var, value);
    }

    Op& ActState(const StateRef state) {
      HASSERT((state.state & GET_INVERSE_MASK(_IF_STATE_MASK)) == state.state);
      acts_.push_back(_ACT_STATE | state.state);
      return *this;
    }

    Op& IfTimerNotDone() {
        return IfReg1(parent_->timer_bit_);
    }

    Op& IfTimerDone() {
        return IfReg0(parent_->timer_bit_);
    }

    Op& ActTimer(int seconds) {
        assert((seconds & _ACT_TIMER_MASK) == 0);
        uint8_t v = _ACT_TIMER;
        v |= seconds;
        acts_.push_back(v);
        return *this;
    }
    
    Op& AddIf(uint8_t byte) {
        ifs_.push_back(byte);
        return *this;
    }
    Op& AddAct(uint8_t byte) {
        acts_.push_back(byte);
        return *this;
    }

  Op& ActSetAspect(uint8_t value) {
    acts_.push_back(_ACT_SET_ASPECT);
    acts_.push_back(value);
    return *this;
  }

  Op& ActUpAspect() {
    acts_.push_back(_ACT_UP_ASPECT);
    return *this;
  }

  Op& IfAspect(uint8_t value) {
    ifs_.push_back(_IF_ASPECT);
    ifs_.push_back(value);
    return *this;
  }

  Op& ActSetValue(Automata::LocalVariable* var, unsigned offset, uint8_t value) {
    acts_.push_back(_ACT_SET_VAR_VALUE);
    HASSERT(offset < 8);
    uint8_t v = offset << 5;
    if (output_) v |= (var->GetId() & 31);
    acts_.push_back(v);
    acts_.push_back(value);
    return *this;
  }

  Op& ActSetValueFromAspect(Automata::LocalVariable* var, unsigned offset) {
    acts_.push_back(_ACT_SET_VAR_VALUE_ASPECT);
    HASSERT(offset < 8);
    uint8_t v = offset << 5;
    if (output_) v |= (var->GetId() & 31);
    acts_.push_back(v);
    return *this;
  }

  Op& ActGetValueToAspect(const Automata::LocalVariable& var, unsigned offset) {
    acts_.push_back(_ACT_GET_VAR_VALUE_ASPECT);
    HASSERT(offset < 8);
    uint8_t v = offset << 5;
    if (output_) v |= (var.GetId() & 31);
    acts_.push_back(v);
    return *this;
  }

  Op& ActGetValueToSpeed(const Automata::LocalVariable& var, unsigned offset) {
    acts_.push_back(_ACT_GET_VAR_VALUE_SPEED);
    HASSERT(offset < 8);
    uint8_t v = offset << 5;
    if (output_) v |= (var.GetId() & 31);
    acts_.push_back(v);
    return *this;
  }

  Op& ActSetId(uint64_t id) {
    acts_.push_back(_ACT_SET_EVENTID);
    // Set event id 0 from 0 with literal bytes.
    acts_.push_back(0b00000111);
    for (int b = 56; b >= 0; b -= 8) {
      acts_.push_back((id >> b) & 0xff);
    }
    return *this;
  }

  /** Sets the speed accumulator. max value of v is 127. */
  Op& ActLoadSpeed(bool is_forward, uint8_t mph) {
    if (mph > 127) mph = 127;
    uint8_t arg = mph;
    if (!is_forward) {
      arg |= 0x80;
    }
    acts_.push_back(_ACT_IMM_SPEED);
    acts_.push_back(arg);
    return *this;
  }

  /** Multiplies the speed accumulator with a value. Value will be
   * downsampled. Gives an error if too small or too high value is
   * requested. negative values will reverse the loco. */
  Op& ActScaleSpeed(float scale) {
    float preround = fabsf(scale) * 32;
    int round = preround;
    HASSERT(round <= 0x7F);
    uint8_t arg = round & 0x7F;
    if (scale < 0) arg |= 0x80;
    acts_.push_back(_ACT_SCALE_SPEED);
    acts_.push_back(arg);
    return *this;
  }

  Op& IfGetSpeed() {
    ifs_.push_back(_GET_TRAIN_SPEED);
    return *this;
  }

  Op& IfSetSpeed() {
    ifs_.push_back(_SET_TRAIN_SPEED);
    return *this;
  }

  Op& IfSpeedIsForward() {
    ifs_.push_back(_IF_FORWARD);
    return *this;
  }

  Op& IfSpeedIsReverse() {
    ifs_.push_back(_IF_REVERSE);
    return *this;
  }

  Op& ActSpeedForward() {
    acts_.push_back(_ACT_SPEED_FORWARD);
    return *this;
  }

  Op& ActSpeedReverse() {
    acts_.push_back(_ACT_SPEED_REVERSE);
    return *this;
  }

  Op& ActDirectionFlip() {
    acts_.push_back(_ACT_SPEED_FLIP);
    return *this;
  }

  Op& IfSetEStop() {
    ifs_.push_back(_IF_EMERGENCY_STOP);
    return *this;
  }

  Op& IfClearEStop() {
    ifs_.push_back(_IF_EMERGENCY_START);
    return *this;
  }

  Op& RunCallback(OpCallback* cb) {
    if (cb) cb->Run(this);
    return *this;
  }

  typedef Op& (Op::*const_var_fn_t)(const Automata::LocalVariable&);
  typedef Op& (Op::*mutable_var_fn_t)(Automata::LocalVariable*);

  template<class C> Op& Rept(mutable_var_fn_t fn,
           const C& vars) {
    for (GlobalVariable* v : vars) {
      (this->*fn)(parent()->ImportVariable(v));
    }
    return *this;
  }

  template<class C> Op& Rept(const_var_fn_t fn,
           const C& vars) {
    for (const GlobalVariable* v : vars) {
      (this->*fn)(parent()->ImportVariable(*v));
    }
    return *this;
  }

  Automata* parent() { return parent_; }

private:
    DISALLOW_COPY_AND_ASSIGN(Op);

    vector<uint8_t> ifs_;
    vector<uint8_t> acts_;

    Automata* parent_;
    // This may be NULL to indicate null output.
    string* output_;

    string file_;
    int line_;
};

/*Automata::Op Automata::Def() {
    return Op(this, output_);
    }*/

}  // namespace automata

#endif // _bracz_train_automata_operations_hxx_
