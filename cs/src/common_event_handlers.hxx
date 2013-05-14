#ifndef __BRACZ_TRAIN_COMMON_EVENT_HANDLERS_HXX_
#define __BRACZ_TRAIN_COMMON_EVENT_HANDLERS_HXX_

#include "event_registry.hxx"


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


class GPIOOutToggleEventHandler : public MemoryToggleEventHandler<uint32_t> {
 public:
  GPIOOutToggleEventHandler(uint64_t event_on, uint64_t event_off,
                            uint32_t mask,
                            uint32_t* memory_on, uint32_t* memory_off)
      : MemoryToggleEventHandler<uint32_t>(
            event_on, event_off, mask, memory_on),
        memory_off_(memory_off) {}

   virtual void HandleEvent(uint64_t event) {
     if (event == event_off_) {
       *memory_off_ |= mask_;
     } else {
       MemoryToggleEventHandler<uint32_t>::HandleEvent(event);
     }
   }
 protected:
  uint32_t* memory_off_;
};




#endif // __BRACZ_TRAIN_COMMON_EVENT_HANDLERS_HXX_
