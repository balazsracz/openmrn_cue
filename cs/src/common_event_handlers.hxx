#ifndef __BRACZ_TRAIN_COMMON_EVENT_HANDLERS_HXX_
#define __BRACZ_TRAIN_COMMON_EVENT_HANDLERS_HXX_

#include "event_registry.hxx"
#include "updater.hxx"
#include "nmranet_types.h"
#include "core/nmranet_event.h"

template <class I> class MemoryToggleEventHandler : public EventHandler {
 public:
  MemoryToggleEventHandler(uint64_t event_on, uint64_t event_off,
                           I mask, I* memory)
      : event_on_(event_on), event_off_(event_off),
        memory_(memory), mask_(mask) {
    EventRegistry::instance()->RegisterHandler(this, event_on_);
    EventRegistry::instance()->RegisterHandler(this, event_off_);
  }

  virtual ~MemoryToggleEventHandler() {
    EventRegistry::instance()->UnregisterHandler(this, event_on_);
    EventRegistry::instance()->UnregisterHandler(this, event_off_);
  }

  virtual void HandleEvent(uint64_t event) {
    if (event == event_on_) {
      *memory_ |= mask_;
    } else if (event == event_off_) {
      *memory_ &= ~mask_;
    } else {
      ASSERT(0);
    }
  }

 protected:
  uint64_t event_on_, event_off_;
  I* memory_;
  I mask_;
};


class GPIOOutToggleEventHandler : public MemoryToggleEventHandler<volatile uint32_t> {
 public:
  GPIOOutToggleEventHandler(uint64_t event_on, uint64_t event_off,
                            uint32_t mask,
                            uint32_t* memory_on, uint32_t* memory_off)
      : MemoryToggleEventHandler<volatile uint32_t>(
            event_on, event_off, mask, memory_on),
        memory_off_(memory_off) {}

   virtual void HandleEvent(uint64_t event) {
     if (event == event_off_) {
       *memory_off_ |= mask_;
     } else {
       MemoryToggleEventHandler<volatile uint32_t>::HandleEvent(event);
     }
   }
 protected:
  volatile uint32_t* memory_off_;
};

/**
   This class exports a memory range as a consumer of a contiguous event
   range. Each bit will have two consecutive events, the first will set the bit
   to 1, the second will set the bit to 0.

   Event IDs are increasing from the specified base event id, LSB bits-first,
   and the byte offsets increasingly.

   Example: MemoryBitSetEventHandler(0x050101012000A0, ptr, 17)
   0x050101012000A0  -> set bit 0 of ptr[0] to 1
   0x050101012000A1  -> set bit 0 of ptr[0] to 0
   0x050101012000A2  -> set bit 1 of ptr[0] to 1
   0x050101012000A3  -> set bit 1 of ptr[0] to 0
   ...
   0x050101012000AE  -> set bit 7 of ptr[0] to 1
   0x050101012000AF  -> set bit 7 of ptr[0] to 0
   0x050101012000B0  -> set bit 7 of ptr[1] to 1
   ...
   0x050101012000C1  -> set bit 0 of ptr[2] to 0
   0x050101012000C2  ignored.

   This class does not advertise the events as consumers, that is the
   responsibility of the caller. (Which is difficult at the moment, because
   event range consumed messages are not supported by OpenMRN yet.)
 */
class MemoryBitSetEventHandler : public EventHandler {
 public:
  MemoryBitSetEventHandler(uint64_t event_base, uint8_t* data, int len_bits)
      : event_(event_base), data_(data), len_bits_(len_bits) {
    EventRegistry::instance()->RegisterGlobalHandler(this);
  }

  virtual void HandleEvent(uint64_t event) {
    if (event < event_) return;
    if (event >= event_ + (len_bits_ * 2)) return;
    int diff = event - event_;
    int ofs = diff >> 4;
    int bit = (diff & 0xf) >> 1;
    if (diff & 1) {
      data_[ofs] &= ~(1<<bit);
    } else {
      data_[ofs] |= (1<<bit);
    }
  }

 private:
  uint64_t event_;
  uint8_t* data_;
  int len_bits_;
};


class MemoryBitSetProducer : public UpdateListener {
 public:
  MemoryBitSetProducer(node_t node, uint64_t event_base)
      : node_(node), event_base_(event_base) {}

  virtual ~MemoryBitSetProducer() {}

  virtual void OnChanged(uint8_t offset, uint8_t previous_value, uint8_t new_value);

 private:
  node_t node_;
  uint64_t event_base_;
};


template <class I> class MemoryBitProducer : public Updatable {
 public:
  MemoryBitProducer(node_t node, uint64_t event_on, uint64_t event_off,
                    I mask, I* memory)
      : node_(node), memory_(memory), mask_(mask),
        last_state_(0), last_state_count_(0) {
    event_[0] = event_off;
    event_[1] = event_on;
    nmranet_event_producer(node, event_[0], EVENT_STATE_INVALID);
    nmranet_event_producer(node, event_[1], EVENT_STATE_INVALID);
  }

  virtual void PerformUpdate() {
    unsigned state = ((*memory_) & mask_) ? 1 : 0;
    if (state == last_state_) {
      if (last_state_count_ < BIT_PRODUCER_UNCHANGED_COUNT) {
        if (++last_state_count_ >= BIT_PRODUCER_UNCHANGED_COUNT) {
          nmranet_event_produce(node_, event_[state], EVENT_STATE_VALID);
          nmranet_event_produce(node_, event_[state ^ 1], EVENT_STATE_INVALID);
        }
      }
    } else {
      last_state_count_ = 0;
      last_state_ = state;
    }
  }

 protected:
  node_t node_;
  uint64_t event_[2];
  I* memory_;
  I mask_;
  unsigned last_state_ : 1;
  unsigned last_state_count_ : 7;
};


#endif // __BRACZ_TRAIN_COMMON_EVENT_HANDLERS_HXX_
