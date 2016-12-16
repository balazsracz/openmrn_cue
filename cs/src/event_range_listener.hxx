#ifndef _BRACZ_TRAIN_EVENT_RANGE_LISTENER_HXX_
#define _BRACZ_TRAIN_EVENT_RANGE_LISTENER_HXX_

#include "updater.hxx"
#include "openlcb/EventHandlerTemplates.hxx"

class ListenerToEventProxy : public UpdateListener {
 public:
  ListenerToEventProxy(openlcb::BitRangeEventPC* proxy)
      : proxy_(proxy) {}

  uint8_t OnChanged(uint8_t offset, uint8_t previous_value, uint8_t new_value) override;

 private:
  openlcb::BitRangeEventPC* proxy_;
};


#endif // _BRACZ_TRAIN_EVENT_RANGE_LISTENER_HXX_
