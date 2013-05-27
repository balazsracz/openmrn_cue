#ifndef _bracz_train_automata_variables_hxx_
#define _bracz_train_automata_variables_hxx_

#include "system.hxx"
#include "operations.hxx"

namespace automata {

/**
   A global variable implementation that uses two events to set a state bit.
 */
class EventBasedVariable : public GlobalVariable {
public:
    EventBasedVariable(Board* brd,
                       uint64_t event_on, uint64_t event_off,
                       int client, int offset, int bit)
        : event_on_(event_on), event_off_(event_off) {
        arg1_ = (0 << 5) | (client & 0b11111);
        arg2_ = (offset << 3) | (bit & 7);
        if (brd) {
            brd->AddVariable(this);
        }
    }
    virtual ~EventBasedVariable() {}

    virtual void Render(string* output) {
        typedef Automata::Op Op;
        vector<uint8_t> empty;
        vector<uint8_t> op;
        op.push_back(_ACT_SET_EVENTID);
        op.push_back(0b01010111);
        for (int b = 56; b >= 0; b -= 8) {
            op.push_back((event_on_ >> b) & 0xff);
        }
        Op::CreateOperation(output, empty, op);
        // TODO(bracz): here we should optimize the used bytes count.
        op.clear();
        op.push_back(_ACT_SET_EVENTID);
        op.push_back(0b00000111);
        for (int b = 56; b >= 0; b -= 8) {
            op.push_back((event_off_ >> b) & 0xff);
        }
        Op::CreateOperation(output, empty, op);
        op.clear();
        op.push_back(_ACT_DEF_VAR);
        op.push_back(arg1_);
        op.push_back(arg2_);
        SetId(output->size());
        Op::CreateOperation(output, empty, op);
    }

private:
    uint64_t event_on_, event_off_;
    uint8_t arg1_;
    uint8_t arg2_;
};

}  // namespace

#endif // _bracz_train_automata_variables_hxx_
