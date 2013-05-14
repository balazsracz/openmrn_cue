#include <map>
#include <algorithm>

#include "cs_config.h"
#include "event_registry.hxx"

using namespace std;

struct EventRegistry::Impl {
  typedef multimap<uint64_t, EventHandler*> Handlers;
  Handlers handlers;
};

EventRegistry::EventRegistry() {
  ASSERT(instance_ == NULL);
  instance_ = this;
  impl_ = new Impl();
}


EventRegistry::~EventRegistry() {
  instance_ = NULL;
  delete impl_;
}

void EventRegistry::RegisterHandler(EventHandler* handler, uint64_t event) {
  impl_->handlers.insert(make_pair(event, handler));
}

void EventRegistry::UnregisterHandler(EventHandler* handler, uint64_t event) {
  auto r = impl_->handlers.equal_range(event);
  for (auto it = r.first; it != r.second;) {
    if (it->second == handler) {
      it = impl_->handlers.erase(it);
    } else {
      ++it;
    }
  }
}

void EventRegistry::HandleEvent(uint64_t event) {
  auto r = impl_->handlers.equal_range(event);
  for (auto it = r.first; it != r.second; it++) {
    it->second->HandleEvent(event);
  }
}

EventRegistry* EventRegistry::instance_ = NULL;
