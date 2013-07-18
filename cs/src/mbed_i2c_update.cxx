#include <stdint.h>
#include <initializer_list>

#include "mbed_i2c_update.hxx"

#ifdef __FreeRTOS__

#include "mbed.h"

void I2COutUpdater::PerformUpdate() {
  uint8_t all_data[preamble_.size() + data_length_];
  memcpy(all_data, preamble_.data(), preamble_.size());
  memcpy(all_data + preamble_.size(), data_offset_, data_length_);
  port_->write(address_ << 1, (const char*)(all_data),
               preamble_.size() + data_length_);
}

void I2CInUpdater::PerformUpdate() {
  int ret;
  if (preamble_.size()) {
    if ((ret = port_->write(address_ << 1, (const char*)preamble_.data(),
                            preamble_.size(), false))) {
      return;
    }
  }
  uint8_t new_data[data_length_];
  if ((ret = port_->read((address_ << 1) | 1,
                         (char*)new_data, data_length_))) {
    return;
  }
  for (int i = 0; i < data_length_; i++) {
    if (shadow_[i] == new_data[i]) {
      if (++count_unchanged_[i] >= I2C_READ_UNCHANGED_COUNT) {
        data_offset_[i] = shadow_[i];
      }
    } else {
      shadow_[i] = new_data[i];
      count_unchanged_[i] = 0;
    }
  }
}

#endif
