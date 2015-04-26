#ifndef _BRACZ_CUSTOM_WIICHUCKREADER_HXX_
#define _BRACZ_CUSTOM_WIICHUCKREADER_HXX_

#include <stdint.h>
#include <fcntl.h>

#include "custom/WiiChuckData.hxx"
#include "os/OS.hxx"
#include "utils/constants.hxx"

DECLARE_CONST(wiichuck_thread_prio);
DECLARE_CONST(wiichuck_thread_stack_size);
DECLARE_CONST(wiichuck_poll_usec);

namespace bracz_custom {

class WiiChuckReader : private OSThread {
public:
  WiiChuckReader(const char* device, WiiChuckConsumerFlowInterface* target)
    : fd_(::open(device, O_RDWR)),
      target_(target) {
    HASSERT(fd_ >= 0);
  }

  WiiChuckReader(int fd, WiiChuckConsumerFlowInterface* target)
    : fd_(fd), target_(target) {}

  void start();

private:
  static const uint8_t WIICHUCK_ADDRESS = 0x52;

  void* entry() OVERRIDE;

  int fd_;
  WiiChuckConsumerFlowInterface* target_;
};

}  // namespace bracz_custom

#endif // _BRACZ_CUSTOM_WIICHUCKREADER_HXX_
