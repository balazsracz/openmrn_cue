#include "event_range_listener.hxx"

#include "executor/Notifiable.hxx"

namespace {
BarrierNotifiable write_helper_busy;
nmranet::WriteHelper helper;
}

uint8_t ListenerToEventProxy::OnChanged(uint8_t offset, uint8_t previous_value,
                                        uint8_t new_value) {
  if (!write_helper_busy.is_done()) {
    // Can't write, so we shouldn't try to diff anything.
    return previous_value;
  }
  uint8_t diffval = previous_value ^ new_value;
  for (int ofs = 7; ofs >= 0; ofs--) {
    uint8_t bit = 1<<ofs;
    if (diffval & bit) {
      // Need to update this bit.
      int full_bit_offset = offset * 8 + ofs;
      proxy_->Set(
          full_bit_offset, new_value & bit, &helper,
          write_helper_busy.reset(EmptyNotifiable::DefaultInstance()));
      // This should flip one more bit in the backing store. We'll update the
      // rest of the bits when the write helper is done.
      previous_value ^= bit;
      return previous_value;
    }
  }
  return new_value;
}
