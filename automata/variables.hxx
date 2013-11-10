#ifndef _bracz_train_automata_variables_hxx_
#define _bracz_train_automata_variables_hxx_

#include "system.hxx"
#include "operations.hxx"

#include "../cs/src/base.h"  // for constants in decodeoffset.

namespace automata {

// Creates the arguments for EventBasedVariable allocation from a contiguous
// counter.
inline bool DecodeOffset(int counter, int* client, int* offset, int* bit) {
  *bit = counter & 7;
  int autid = counter >> 3;
  *client = autid >> 3;
  int autofs = autid & 7;
  *offset = autofs * LEN_AUTOMATA + OFS_BITS;
  return (*client < 8);
}

class EventVariableBase : public GlobalVariable {
 public:
  EventVariableBase(Board* brd) {
    if (brd) {
      brd->AddVariable(this);
    }
  }

  void RenderHelper(string* output) {
    // We take the lead byte of op and the _ACT_DEF_VAR byte into account.
    SetId(output->size() + 2);

    vector<uint8_t> empty;
    vector<uint8_t> op;
    op.push_back(_ACT_DEF_VAR);
    op.push_back(arg1_);
    op.push_back(arg2_);
    Automata::Op::CreateOperation(output, empty, op);
  }

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

 protected:
  uint8_t arg1_;
  uint8_t arg2_;
};

/**
   A global variable implementation that uses two events to set a state bit.
*/
class EventBasedVariable : public EventVariableBase {
 public:
  EventBasedVariable(Board* brd, uint64_t event_on, uint64_t event_off,
                     int counter)
      : EventVariableBase(brd), event_on_(event_on), event_off_(event_off) {
    int client, offset, bit;
    HASSERT(DecodeOffset(counter, &client, &offset, &bit));
    SetArgs(client, offset, bit);
  }

  EventBasedVariable(Board* brd, uint64_t event_on, uint64_t event_off,
                     int client, int offset, int bit)
      : EventVariableBase(brd), event_on_(event_on), event_off_(event_off) {
    SetArgs(client, offset, bit);
  }
  virtual ~EventBasedVariable() {}

  virtual uint64_t event_on() const { return event_on_; }
  virtual uint64_t event_off() const { return event_off_; }

  virtual void Render(string* output) {
    CreateEventId(1, event_on_, output);
    CreateEventId(0, event_off_, output);
    RenderHelper(output);
  }

 private:
  void SetArgs(int client, int offset, int bit) {
    arg1_ = (0 << 5) | (client & 0b11111);
    arg2_ = (offset << 3) | (bit & 7);
  }

  uint64_t event_on_, event_off_;
};

class EventBlock : public EventVariableBase {
 public:
  EventBlock(Board* brd, uint64_t event_base, const string& name)
      : EventVariableBase(brd),
        event_base_(event_base),
        size_(1),
        allocator_(this, name) {}

  virtual void Render(string* output) {
    CreateEventId(0, event_base_, output);
    HASSERT(size_ < (8 << 8));
    arg1_ = (1 << 5) | ((size_ >> 8) & 7);
    arg2_ = size_ & 0xff;
    RenderHelper(output);
  }

  void SetMinSize(size_t size) {
    if (size >= size_) size_ = size + 1;
  }

  class Allocator {
   public:
    Allocator(EventBlock* block, const string& name)
        : name_(name), block_(block), next_entry_(0), end_(8 << 8) {}

    Allocator(const Allocator* parent, const string& name, int count,
              int alignment = 1)
        : name_(CreateNameFromParent(parent, name)), block_(parent->block_) {
      parent->Align(alignment);
      next_entry_ = parent->Reserve(count);
      end_ = next_entry_ + count;
    }

    // Reserves a number entries at the beginning of the block. Returns the
    // first entry that was reserved.
    int Reserve(int count) const {
      HASSERT(next_entry_ + count <= end_);
      int ret = next_entry_;
      next_entry_ += count;
      return ret;
    }

    // Creates a new variable and transfers ownership to the caller.
    GlobalVariable* Allocate(const string& name) const;

    // Rounds up the next to-be-allocated entry to Alignment.
    void Align(int alignment) const {
      next_entry_ += alignment - 1;
      next_entry_ /= alignment;
      next_entry_ *= alignment;
    }

    const string& name() const { return name_; }

    EventBlock* block() const { return block_; }

    int remaining() const { return end_ - next_entry_; }

    // Concatenates parent->name() and 'name', adding a '.' as separator if both
    // are non-empty.
    static string CreateNameFromParent(const Allocator* parent, const string& name) {
      if (name.empty()) return parent->name();
      if (parent->name().empty()) return name;
      string ret = parent->name();
      ret += ".";
      ret += name;
      return ret;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(Allocator);
    string name_;
    EventBlock* block_;
    mutable int next_entry_;
    int end_;
  };

  Allocator* allocator() { return &allocator_; }

  uint64_t event_base() const { return event_base_; }

  // These should never be called. This variable does not represent a single
  // bit.
  virtual uint64_t event_on() const { HASSERT(false); }
  virtual uint64_t event_off() const { HASSERT(false); }

 private:
  DISALLOW_COPY_AND_ASSIGN(EventBlock);
  uint64_t event_base_;
  uint16_t size_;
  Allocator allocator_;
};

class BlockVariable : public GlobalVariable {
 public:
  BlockVariable(const EventBlock::Allocator* allocator, const string& name)
      : parent_(allocator->block()),
        name_(allocator->CreateNameFromParent(allocator, name)) {
    int arg = allocator->Reserve(1);
    SetArg(arg);
    parent_->SetMinSize(arg);
  }

  virtual void Render(string* output) {
    // Block bits do not need any rendering. The render method will never be
    // called.
    HASSERT(0);
  }

  virtual GlobalVariableId GetId() const {
    GlobalVariableId tmp_id = parent_->GetId();
    tmp_id.arg = id_.arg;
    return tmp_id;
  }

  const string& name() { return name_; }

  virtual uint64_t event_on() const {
    return parent_->event_base() + id_.arg * 2;
  }

  virtual uint64_t event_off() const {
    return parent_->event_base() + id_.arg * 2 + 1;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BlockVariable);
  EventBlock* parent_;
  string name_;
};

}  // namespace

#endif  // _bracz_train_automata_variables_hxx_
