#ifndef _BRACZ_TRAIN_I2C_EXTENDER_FLOW_HXX_
#define _BRACZ_TRAIN_I2C_EXTENDER_FLOW_HXX_

#include "src/i2c_driver.hxx"
#include "nmranet/EventHandlerTemplates.hxx"

extern I2CDriver g_i2c_driver;

extern "C" { void resetblink(uint32_t pattern); }

namespace nmranet {
class AsyncNode;
}

static const int DATA_REPEAT_COUNT = 3;
static nmranet::WriteHelper g_i2c_write_helper;

class I2cExtenderBoard : public ControlFlow {
public:
  I2cExtenderBoard(I2CDriver* driver, uint8_t address, Executor *executor,
                   nmranet::AsyncNode *node)
      : ControlFlow(executor, nullptr), driver_(driver), address_(address),
        bit_pc_(node, BRACZ_LAYOUT | (address << 8), &io_store_, 24),
        signal_c_(node, (BRACZ_LAYOUT & (~0xFFFFFF)) | (address << 16), signal_data_, sizeof(signal_data_)) {
    io_store_ = 1;
    // This code only works for little-endian otherwise the bitrange-eventpc
    // will export different bytes than what we read from i2c.
    HASSERT(io_data_[0] == 1);
    io_store_ = 0;
    memset(signal_data_, 1, sizeof(signal_data_));
    start_flow(STATE(GetHardware));
  }

  Action GetHardware() {
    return Allocate(driver_->allocator(), STATE(StartReceive));
  }

  Action StartReceive() {
    driver_->StartRead(address_, 0, 1, this);
    return WaitAndCall(STATE(ReadDone));
  }

  Action ReadDone() {
    if (driver_->success()) {
      if (driver_->read_buffer()[0] == new_data_) {
        // Checks if we had enough repeats to call this data stable.
        if (++data_count_ >= DATA_REPEAT_COUNT) {
          // Prevents overflow.
          data_count_ = DATA_REPEAT_COUNT;
          return yield_and_call(STATE(UpdateIncomingData));
        }
      } else {
        // Data changed, start over counting.
        data_count_ = 0;
        new_data_ = driver_->read_buffer()[0];
      }
      return yield_and_call(STATE(StartSend));
    } else {
      // Failure to read, start over counting and try again later.
      resetblink(0x80000A02);
      data_count_ = 0;
      driver_->Release();
      return yield_and_call(STATE(GetHardware));
    }
  }

  Action UpdateIncomingData() {
    for (int i = 0; i < 8; ++i) {
      bool old_state = bit_pc_.Get(kReadBitOffset + i);
      bool new_state = new_data_ & (1 << i);
      if (new_state != old_state) {
        bit_pc_.Set(kReadBitOffset + i, new_state, &g_i2c_write_helper, this);
        return wait_for_notification();
      }
    }
    return call_immediately(STATE(StartSend));
  }

  Action StartSend() {
    uint8_t *buf = driver_->write_buffer();
    buf[0] = io_data_[0];
    buf[1] = io_data_[1];
    memcpy(buf + 2, signal_data_, sizeof(signal_data_));
    driver_->StartWrite(address_, 2 + sizeof(signal_data_), this);
    return WaitAndCall(STATE(WriteDone));
  }

  Action WriteDone() {
    if (driver_->success()) {
      resetblink(0);
    } else {
      resetblink(0x80000A02);
    }
    driver_->Release();
    return call_immediately(STATE(GetHardware));
  }

private:
  static const int kReadBitOffset = 16;
  static const int kReadByteOffset = 2;

  I2CDriver* driver_;
  // I2C target address
  uint8_t address_;
  // Freshly read data.
  uint8_t new_data_;
  // How many times have we seen the same incoming data.
  uint8_t data_count_;
  // Stable version of the data. offset 0-1 is outgoing bit-based data; offset
  // 2 is the stable incoming data.
  union {
    uint8_t io_data_[4];
    uint32_t io_store_;
  };

  uint8_t signal_data_[EXT_SIGNAL_COUNT * 2];

  nmranet::BitRangeEventPC bit_pc_;
  nmranet::ByteRangeEventC signal_c_;
};

#endif // _BRACZ_TRAIN_I2C_EXTENDER_FLOW_HXX_
