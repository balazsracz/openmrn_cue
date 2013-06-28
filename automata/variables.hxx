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

  // Creates an operation setting a particular eventid.
  //
  // dst is the index of eventid to set (0 or 1).
  static void CreateEventId(int dst, uint64_t eventid, string* output) {
    vector<uint8_t> empty;
    vector<uint8_t> op;
    op.push_back(_ACT_SET_EVENTID);
    if (dst) {
      op.push_back(0b01010111);
    } else {
      op.push_back(0b00000111);
    }
    for (int b = 56; b >= 0; b -= 8) {
      op.push_back((eventid >> b) & 0xff);
    }
    Automata::Op::CreateOperation(output, empty, op);
  }

  virtual void Render(string* output) {
    CreateEventId(1, event_on_, output);
    CreateEventId(0, event_off_, output);

    // We take the lead byte of op and the _ACT_DEF_VAR byte into account.
    SetId(output->size() + 2);

    vector<uint8_t> empty;
    vector<uint8_t> op;
    op.push_back(_ACT_DEF_VAR);
    op.push_back(arg1_);
    op.push_back(arg2_);
    Automata::Op::CreateOperation(output, empty, op);
  }

 private:
  uint64_t event_on_, event_off_;
  uint8_t arg1_;
  uint8_t arg2_;
};

}  // namespace

#endif // _bracz_train_automata_variables_hxx_
