#include "custom/WiiChuckReader.hxx"

#include <fcntl.h>
#include <sys/ioctl.h>

#include "i2c.h"
#include "i2c-dev.h"

namespace bracz_custom {

static const uint8_t INIT_DATA[] = {0xF0, 0x55};
static const uint8_t MEASURE_DATA[] = {0x00};

void* WiiChuckReader::entry() {
  usleep(500000);
  // Initialize wii chuck
  ERRNOCHECK("init_set_slave", ::ioctl(fd_, I2C_SLAVE, WIICHUCK_ADDRESS));
  ERRNOCHECK("init_write", ::write(fd_, INIT_DATA, sizeof(INIT_DATA)));
  while (true) {
    usleep(config_wiichuck_poll_usec());
    ERRNOCHECK("measure_write",
               ::write(fd_, MEASURE_DATA, sizeof(MEASURE_DATA)));
    auto* b = target_->alloc();
    usleep(2000);
    ERRNOCHECK("measure_read", ::read(fd_, b->data()->data, 6));
    target_->send(b);
  }
  return nullptr;
}

void WiiChuckReader::start() {
  OSThread::start("wiichuck_read", (int) config_wiichuck_thread_prio(),
        (size_t) config_wiichuck_thread_stack_size());
}

}  // namespace bracz_custom
