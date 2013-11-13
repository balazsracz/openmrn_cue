#include "cs_config.h"
#include "common_event_handlers.hxx"
#include "core/nmranet_event.h"


uint8_t MemoryBitSetProducer::OnChanged(uint8_t offset, uint8_t previous_value, uint8_t new_value) {
  uint64_t eventid = event_base_ + offset * 16;
  for (int mask = 1; mask <= 255; mask <<= 1, eventid += 2) {
    if ((previous_value & mask) != (new_value & mask)) {
      if (new_value & mask) {
        nmranet_event_produce(node_, eventid, EVENT_STATE_VALID);
        nmranet_event_produce(node_, eventid + 1, EVENT_STATE_INVALID);
      } else {
        nmranet_event_produce(node_, eventid, EVENT_STATE_INVALID);
        nmranet_event_produce(node_, eventid + 1, EVENT_STATE_VALID);
      }
    }
  }
  return new_value;
}
