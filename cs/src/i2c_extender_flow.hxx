#ifndef _BRACZ_TRAIN_I2C_EXTENDER_FLOW_HXX_
#define _BRACZ_TRAIN_I2C_EXTENDER_FLOW_HXX_

#include "src/i2c_driver.hxx"

extern Executor g_executor;
extern I2CDriver g_i2c_driver;
extern NMRAnet::AsyncNode g_node;

static const int DATA_REPEAT_COUNT = 3;
static NMRAnet::WriteHelper g_i2c_write_helper;

class I2cExtenderBoard : public ControlFlow {
public:
  I2cExtenderBoard(uint8_t address, Executor *executor,
                   NMRAnet::AsyncNode *node)
      : ControlFlow(executor, nullptr), address_(address),
        bit_pc_(node, BRACZ_LAYOUT | (address << 8), io_data_, 24) {
    StartFlowAt(ST(GetHardware));
  }

  ControlFlowAction GetHardware() {
    return Allocate(g_i2c_driver.allocator(), ST(StartReceive));
  }

  ControlFlowAction StartReceive() {
    g_i2c_driver.StartRead(address_, 0, 1, this);
    return WaitAndCall(ST(ReadDone));
  }

  ControlFlowAction ReadDone() {
    if (g_i2c_driver.success()) {
      new_data_ = g_i2c_driver.read_buffer()[0];
      if (new_data_ == io_data_[kReadByteOffset]) {
        // Checks if we had enough repeats to call this data stable.
        if (++data_count_ == DATA_REPEAT_COUNT) {
          return YieldAndCall(ST(UpdateIncomingData));
        }
        // Prevents overflow.
        if (data_count_ > DATA_REPEAT_COUNT) {
          data_count_ = DATA_REPEAT_COUNT;
        }
      } else {
        // Data changed, start over counting.
        data_count_ = 0;
      }
      return YieldAndCall(ST(StartSend));
    } else {
      // Failure to read, start over counting and try again later.
      data_count_ = 0;
      g_i2c_driver.Release();
      return YieldAndCall(ST(GetHardware));
    }
  }

  ControlFlowAction UpdateIncomingData() {
    for (int i = 0; i < 8; ++i) {
      bool old_state = bit_pc_.Get(kReadBitOffset + i);
      bool new_state = new_data_ & (1 << i);
      if (new_state != old_state) {
        bit_pc_.Set(kReadBitOffset + i, new_state, &g_i2c_write_helper, this);
        return WaitForNotification();
      }
    }
    return CallImmediately(ST(StartSend));
  }

  ControlFlowAction StartSend() {
    static int count = 0;
    uint8_t *buf = g_i2c_driver.write_buffer();
    buf[0] = io_data_[0];
    buf[1] = io_data_[1];
    g_i2c_driver.StartWrite(address_, 2, this);
    return WaitAndCall(ST(WriteDone));
  }

  ControlFlowAction WriteDone() {
    if (!g_i2c_driver.success()) {
      resetblink(1);
    }
    g_i2c_driver.Release();
    return CallImmediately(ST(GetHardware));
  }

private:
  static const int kReadBitOffset = 16;
  static const int kReadByteOffset = 2;

  // I2C target address
  uint8_t address_;
  // Freshly read data.
  uint8_t new_data_;
  // How many times have we seen the same incoming data.
  uint8_t data_count_;
  // Stable version of the data. offset 0-1 is outgoing bit-based data; offset
  // 2 is the stable incoming data.
  uint8_t io_data_[3];
  BitRangeEventPC bit_pc_;
};

#endif // _BRACZ_TRAIN_I2C_EXTENDER_FLOW_HXX_
