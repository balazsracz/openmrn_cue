#ifndef _BRACZ_TRAIN_MBED_I2C_UPDATE_HXX_
#define _BRACZ_TRAIN_MBED_I2C_UPDATE_HXX_

#include <stdint.h>
#include <string.h>
#include <initializer_list>
#include <ext/slist>

#include "cs_config.h"

#ifdef HAVE_MBED

#include "updater.hxx"


// Forward declared from the mbed library.
namespace mbed {
class I2C;
}
using mbed::I2C;


class I2CUpdaterBase : public Updatable {
public:
  I2CUpdaterBase(I2C* port, int address, std::initializer_list<uint8_t> preamble, uint8_t* data_offset, int data_length, int extra_data_length);

  virtual ~I2CUpdaterBase() {}

protected:
  uint8_t* preamble() {
    return storage_;
  }

  uint8_t* extra_data() {
    return storage_ + preamble_size_;
  }

  // Port via which to contact the slave board.
  I2C* port_;
  // Data content will be taken from this memory.
  uint8_t* data_offset_;
  // The data is laid out in consecutive bytes here. First preamble_length_
  // bytes for the preamble, then the extra_data for the child.
  uint8_t* storage_;
  // 7-bit I2C address of the slave board.
  short int address_;
  // This many data bytes will be sent.
  uint8_t data_length_;
  // Length of preamble inside storage_.
  uint8_t preamble_size_;
};


class I2COutUpdater : public I2CUpdaterBase {
public:
  I2COutUpdater(I2C* port, int address,
                std::initializer_list<uint8_t> preamble,
                uint8_t* data_offset, int data_length)
      : I2CUpdaterBase(port, address, preamble, data_offset, data_length, 0) {} 

  virtual ~I2COutUpdater() {}

  virtual void PerformUpdate();
};

class I2CInUpdater : public I2CUpdaterBase {
public:
  I2CInUpdater(I2C* port, int address,
               std::initializer_list<uint8_t> preamble,
               uint8_t* data_offset, int data_length)
      : I2CUpdaterBase(port, address, preamble, data_offset, data_length, 2*data_length) {
    memset(extra_data(), 0, 2 * data_length);
  }

  virtual ~I2CInUpdater() {}

  virtual void PerformUpdate();

  void RegisterListener(UpdateListener* listener) {
    listeners_.push_front(listener);
  }

private:
  uint8_t& shadow(int i) {
    return extra_data()[i];
  }

  uint8_t& count_unchanged(int i) {
    return extra_data()[data_length_ + i];
  }

  std::vector<uint8_t> shadow_;
  std::vector<int> count_unchanged_;
  __gnu_cxx::slist<UpdateListener*> listeners_;
};

#endif // HAVE_MBED

#endif // _BRACZ_TRAIN_MBED_I2C_UPDATE_HXX_
