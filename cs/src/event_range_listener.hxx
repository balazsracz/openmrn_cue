#ifndef _BRACZ_TRAIN_EVENT_RANGE_LISTENER_HXX_
#define _BRACZ_TRAIN_EVENT_RANGE_LISTENER_HXX_

#include "updater.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "executor/control_flow.hxx"

class ListenerToEventProxy : public UpdateListener {
 public:
  ListenerToEventProxy(NMRAnet::BitRangeEventPC* proxy)
      : proxy_(proxy) {}

  virtual uint8_t OnChanged(uint8_t offset, uint8_t previous_value, uint8_t new_value);

 private:
  NMRAnet::BitRangeEventPC* proxy_;
};


#endif // _BRACZ_TRAIN_EVENT_RANGE_LISTENER_HXX_
