#include "logging.h"
#include "automata_control.h"

#ifdef nnn__FreeRTOS__

#include "mbed.h"
#include "base.h"

#include "os/os.h"

//I2C i2c(P0_27, P0_28);
I2C i2c(P0_10, P0_11);

void UpdateSlaves(void) {
  uint8_t* b = get_state_byte(1, OFS_IOA);
  int address = 0x25;
  int r;
  r = i2c.write(address<<1, (const char*) b, 1);
  LOG(VERBOSE, "i2c write: addr 0x%02x, value %d, return %d", address, *b, r); 
  address = 0x24;
  b = get_state_byte(2, OFS_IOA);
  r = i2c.write(address<<1, (const char*) b, 1);
  LOG(VERBOSE, "i2c write: addr 0x%02x, value %d, return %d", address, *b, r); 
}

#else
void UpdateSlaves(void) {}

#endif
