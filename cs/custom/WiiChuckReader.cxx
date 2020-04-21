#define _DEFAULT_SOURCE // for usleep
#if defined(__linux__) || defined(__FreeRTOS__)
#define ENABLE_WIICHUCK
#endif

#ifdef ENABLE_WIICHUCK

#define LOGLEVEL INFO

#include "custom/WiiChuckReader.hxx"

#include <fcntl.h>
#ifdef __linux__
#include <sys/ioctl.h>
#else

#endif

#include "i2c.h"
#include "i2c-dev.h"
#include "utils/logging.h"

namespace bracz_custom {

static const uint8_t INIT_DATA[] = {0xF0, 0x55};
static const uint8_t MEASURE_DATA[] = {0x00};

volatile int WiiChuckReader::saved_errno = 0;

void* WiiChuckReader::entry() {
  int fd = -1;
  while (true) {
    if (fd >= 0) ::close(fd);
    saved_errno = 0;
    usleep(500000);
    int fd = ::open(device_, O_RDWR);
    HASSERT(fd >= 0);
    int ret;
    // Initialize wii chuck
    ret = ::ioctl(fd, I2C_SLAVE, WIICHUCK_ADDRESS);
    if (ret < 0) {
      saved_errno = errno;
      continue;
    }
    ret = ::write(fd, INIT_DATA, sizeof(INIT_DATA));
    if (ret < 0) {
      saved_errno = errno;
      continue;
    }
    if (ret < (int)sizeof(INIT_DATA)) {
      saved_errno = errno;
      continue;
    }
    usleep(2000);
    while (true) {
      usleep(config_wiichuck_poll_usec());
      ret = ::write(fd, MEASURE_DATA, sizeof(MEASURE_DATA));
      if (ret < (int)sizeof(MEASURE_DATA)) {
        saved_errno = errno;
        break;
      }
      auto* b = target_->alloc();
      usleep(2000);
      ret = ::read(fd, b->data()->data, 6);
      if (ret < 6) {
        saved_errno = errno;
        b->unref();
        break;
      }
      target_->send(b);
    }
    auto* b = target_->alloc();
    memset(b->data()->data, 0x80, 6);
    target_->send(b);
  }
  return nullptr;
}

void WiiChuckReader::start() {
  OSThread::start("wiichuck_read", (int)config_wiichuck_thread_prio(),
                  (size_t)config_wiichuck_thread_stack_size());
}

}  // namespace bracz_custom

#endif
