#ifndef _bracz_train_automata_operations_hxx_
#define _bracz_train_automata_operations_hxx_

#include "system.hxx"
#include "../cs/src/automata_defs.h"

namespace automata {

#define GET_INVERSE_MASK(x) ( (~(x)) & ((x) - 1) ) 

struct StateRef {
    StateRef(int id) {
        assert((id & GET_INVERSE_MASK(_IF_STATE_MASK)) == id);
        state = id;
    }
    int state;
};

class Automata::Op {
public:
    Op(Automata* parent, string* output)
        : parent_(parent), output_(output) {}

    ~Op() {
        assert(ifs_.size() < 16);
        assert(acts_.size() < 16);
        if (!output_) return;
        uint8_t hdr = 0;
        hdr = (acts_.size() & 0xf) << 4;
        hdr |= ifs_.size() & 0xf;
        output_->push_back(hdr);
        output_->append((char*)ifs_.data(), ifs_.size());
        output_->append((char*)acts_.data(), acts_.size());
    }

    Op& IfState(StateRef& state) {
        ifs_.push_back(_IF_STATE | state.state);
        return *this;
    }

    Op& IfReg0(Automata::LocalVariable& var) {
        uint8_t v = _IF_REG;
        if (output_) v |= var.GetId();
        ifs_.push_back(v);
        return *this;
    }

    Op& IfReg1(Automata::LocalVariable& var) {
        uint8_t v = _IF_REG | _REG_1;
        if (output_) v |= var.GetId();
        ifs_.push_back(v);
        return *this;
    }

    Op& ActState(StateRef& state) {
        acts_.push_back(_ACT_STATE | state.state);
        return *this;
    }
    
private:
    DISALLOW_COPY_AND_ASSIGN(Op);

    vector<uint8_t> ifs_;
    vector<uint8_t> acts_;

    Automata* parent_;
    // This may be NULL to indicate null output.
    string* output_;
};

/*Automata::Op Automata::Def() {
    return Op(this, output_);
    }*/

}  // namespace automata

#endif // _bracz_train_automata_operations_hxx_

