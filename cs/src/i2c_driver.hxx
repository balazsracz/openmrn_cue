
#ifndef _BRACZ_TRAIN_I2C_DRIVER_HXX_
#define _BRACZ_TRAIN_I2C_DRIVER_HXX_

#include "executor/allocator.hxx"


class I2CDriver : public Executable {
public:
  static const int kMaxWriteSize = 50;
  static const int kMaxReadSize = 4;

  I2CDriver();

  TypedAllocator<I2CDriver> *allocator() { return &allocator_; }

  uint8_t* write_buffer() { return write_bytes_; }
  uint8_t* read_buffer() { return read_bytes_; }

  void StartWrite(uint8_t address, uint8_t len, Notifiable* done);

  void StartRead(uint8_t address, uint8_t write_len, uint8_t read_len, Notifiable* done);

  // Re-initialized the I2C hardware. Should be used in case of a timeout.
  void Reset();

  // Callback on the (global) executor that calls the done closure and releases
  // *this.
  virtual void Run();

  bool success() {
    return success_;
  }

  void Release() {
    allocator_.TypedRelease(this);
  }

 public:
  static I2CDriver* instance_;
  static long long Timeout(void*, void*);
  // called by the interrupt service routine.
  void isr();

 private:
  // Called by the constructor to initialize the hardware. IRQn() and dev() is
  // already setup.
  void InitHardware();

  // Returns the NVIC interrupt request number for this instance.
  ::IRQn IRQn();

  // Returns the hardware address for the device.
  LPC_I2C_TypeDef* dev();

  // Initiates a transaction: creates a start condition, sends address, etc.
  void StartTransaction();
  void TransferDoneFromISR(bool succeeded);

  // Slave address to read/write from/to.
  uint8_t address_;
  // Write buffer.
  uint8_t write_bytes_[kMaxWriteSize];
  uint8_t read_bytes_[kMaxReadSize];

  // Number bytes to write.
  uint8_t write_len_;
  // Next byte to send from write_bytes_ array.
  uint8_t write_offset_;
  // Number of bytes to read.
  uint8_t read_len_;
  // Next byte to receive to read_bytes_ array.
  uint8_t read_offset_;

  // non-zero if transfer succeeded.
  uint8_t success_ : 1;

  // This is set to 1 if a timeout tick has happened since the last ISR.
  uint8_t timeout_ : 1;
  // NULL if no transaction is pending.
  Notifiable* done_;
  // Allocator that owns *this and hands out authorization to use the I2C
  // hardware.
  TypedAllocator<I2CDriver> allocator_;
};

#endif // _BRACZ_TRAIN_I2C_DRIVER_HXX_
