#ifndef __BRACZ_TRAIN_EVENT_REGISTRY_HXX_
#define __BRACZ_TRAIN_EVENT_REGISTRY_HXX_

#include <stdint.h>

#include "cs_config.h"

class EventHandler {
 public:
  virtual void HandleEvent(uint64_t event) = 0;
  virtual ~EventHandler() {}
};

class EventRegistry {
 public:
  //! Executes all event handlers for event.
  void HandleEvent(uint64_t event);

  //! Adds a new event handler. The caller retains ownership of handler.
  void RegisterHandler(EventHandler* handler, uint64_t event);

  //! Removes handler from handling event.
  void UnregisterHandler(EventHandler* handler, uint64_t event);

  //! Adds a new global event handler. This will be called with every incoming
  //! event. The caller retains ownership of handler.
  void RegisterGlobalHandler(EventHandler* handler) {
    RegisterHandler(handler, 0);
  }

  //! Removes handler from handling event.
  void UnregisterGlobalHandler(EventHandler* handler) {
    UnregisterHandler(handler, 0);
  }

  static EventRegistry* instance() {
    ASSERT(instance_);
    return instance_;
  }

  EventRegistry();
  ~EventRegistry();

 private:
  static EventRegistry* instance_;

  struct Impl;

  Impl* impl_;
};

#endif // __BRACZ_TRAIN_EVENT_REGISTRY_HXX_
