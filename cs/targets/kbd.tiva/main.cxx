


#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "os/os.h"
#include "utils/blinker.h"
#include "hardware.hxx"
#include "executor/StateFlow.hxx"

Executor<1> g_executor("executor", 0, 2048);
Service g_service(&g_executor);

string d("Hello, World!0123456789012345678899012312312908234091235789324896459384765923485761302485763294587612349583\n12345\n210");

string e("\b\b\b765\n");

class TestFlow : public StateFlowBase {
 public:
  TestFlow() : StateFlowBase(&g_service) {
    start_flow(STATE(wait_for_pin));
  }

  void init() {
    kbdFd_ = ::open("/dev/kbd", O_RDWR);
    HASSERT(kbdFd_ >= 0);
  }

  Action wait_for_pin() {
    if (SW1_Pin::get()) {
      return sleep_and_call(&timer_, MSEC_TO_NSEC(10), STATE(wait_for_pin));
    }
    return call_immediately(STATE(type_text));
  }

  Action type_text() {
    return write_repeated(&selectHelper_, kbdFd_, d.data(), d.size(), STATE(wait_backspace));
  }

  Action wait_backspace() {
    return sleep_and_call(&timer_, SEC_TO_NSEC(2), STATE(try_backspace));
  }

  Action try_backspace() {
    return write_repeated(&selectHelper_, kbdFd_, e.data(), e.size(), STATE(type_complete));
  }

  Action type_complete() {
    if (!SW1_Pin::get()) {
      return sleep_and_call(&timer_, MSEC_TO_NSEC(10), STATE(type_complete));
    }
    return sleep_and_call(&timer_, MSEC_TO_NSEC(100), STATE(wait_for_pin));
  }

 private:
  int kbdFd_{-1};
  StateFlowSelectHelper selectHelper_{this};
  StateFlowTimer timer_{this};
} g_test_flow;

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[])
{
  g_test_flow.init();
  setblink(0);
  while (1)
  {
    usleep(3000000);
    resetblink(1);
    usleep(50000);
    resetblink(0);
  }
  return 0;
}
