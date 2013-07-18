#ifndef _BRACZ_TRAIN_MBED_I2C_UPDATE_HXX_
#define _BRACZ_TRAIN_MBED_I2C_UPDATE_HXX_

#include <stdint.h>
#include <initializer_list>

#include "updater.hxx"


// Forward declared from the mbed library.
namespace mbed {
class I2C;
}
using mbed::I2C;


class I2CUpdaterBase : public Updatable {
public:
  I2CUpdaterBase(I2C* port, int address, std::initializer_list<uint8_t> preamble, uint8_t* data_offset, int data_length)
    : port_(port), address_(address), preamble_(preamble),
      data_offset_(data_offset), data_length_(data_length) {}

  virtual ~I2CUpdaterBase() {}

protected:
  // Port via which to contact the slave board.
  I2C* port_;
  // 7-bit I2C address of the slave board.
  int address_;
  // These bytes will be written after the address before the data content.
  std::vector<uint8_t> preamble_;
  // Data content will be taken from this memory.
  uint8_t* data_offset_;
  // This many data bytes will be sent.
  int data_length_;
};


class I2COutUpdater : public I2CUpdaterBase {
public:
  I2COutUpdater(I2C* port, int address,
                std::initializer_list<uint8_t> preamble,
                uint8_t* data_offset, int data_length)
    : I2CUpdaterBase(port, address, preamble, data_offset, data_length) {} 

  virtual ~I2COutUpdater() {}

  virtual void PerformUpdate();
};


class I2CInUpdater : public I2CUpdaterBase {
public:
  I2CInUpdater(I2C* port, int address,
               std::initializer_list<uint8_t> preamble,
               uint8_t* data_offset, int data_length)
    : I2CUpdaterBase(port, address, preamble, data_offset, data_length),
      shadow_(data_length, 0),
      count_unchanged_(data_length, 0) {} 

  virtual ~I2CInUpdater() {}

  virtual void PerformUpdate();

private:
  std::vector<uint8_t> shadow_;
  std::vector<int> count_unchanged_;
};

#endif // _BRACZ_TRAIN_MBED_I2C_UPDATE_HXX_
