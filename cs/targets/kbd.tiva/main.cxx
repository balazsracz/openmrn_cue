

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "executor/StateFlow.hxx"
#include "hardware.hxx"
#include "os/os.h"
#include "true_text.h"
#include "utils/blinker.h"
#include "utils/Debouncer.hxx"

Executor<1> g_executor("executor", 0, 2048);
Service g_service(&g_executor);

string d(TEXT_TYPE);

string e("\b\b\b765\n");

class TestFlow : public StateFlowBase {
 public:
  TestFlow() : StateFlowBase(&g_service) { start_flow(STATE(wait_for_pin)); }

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
    return write_repeated(&selectHelper_, kbdFd_, d.data(), d.size(),
                          STATE(wait_backspace));
  }

  Action wait_backspace() {
    return sleep_and_call(&timer_, SEC_TO_NSEC(2), STATE(try_backspace));
  }

  Action try_backspace() {
    return write_repeated(&selectHelper_, kbdFd_, e.data(), e.size(),
                          STATE(type_complete));
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
};

class CommandFlow : public StateFlowBase {
 public:
  CommandFlow() : StateFlowBase(&g_service) {
    start_flow(STATE(wait_for_test));
    io_.push_back(BUT0_Pin::instance());
    io_.push_back(BUT1_Pin::instance());
    io_.push_back(BUT2_Pin::instance());
    QuiesceDebouncer::Options opts(10);
    debs_.emplace_back(opts);
    debs_.emplace_back(opts);
    debs_.emplace_back(opts);
  }

  void init() {
    fd_ = ::open("/dev/ser0", O_RDWR);
    HASSERT(fd_ >= 0);
  }

  Action wait_for_test() {
    if (!SW1_Pin::get()) {
      return call_immediately(STATE(test_phase));
    }
    return sleep_and_call(&timer_, MSEC_TO_NSEC(10), STATE(wait_for_test));
  }

  Action test_phase() {
    LED_GREEN_Pin::set(1);
    LED_RED_Pin::set(0);
    return display_string("Test mode.\n", STATE(run_test_mode));
  }

  Action run_test_mode() {
    if (!SW2_Pin::get()) {
      return call_immediately(STATE(prod_mode));
    }
    for (unsigned i = 0; i < io_.size(); ++i) {
      bool st = io_[i]->is_set();
      if (debs_[i].update_state(st)) {
        // we just saw a change.
        buttonState_ = buttonState_ & ~(1 << i);
        if (st) {
          buttonState_ = buttonState_ | (1 << i);
        }
        return call_immediately(STATE(display_state));
      }
    }
    return sleep_and_call(&timer_, MSEC_TO_NSEC(1), STATE(run_test_mode));
  }

  Action display_state() {
    display_.clear();
    display_.reserve(io_.size() + 1);
    display_.push_back('\r');
    for(unsigned i = 0; i < io_.size(); ++i) {
      if (buttonState_ & (1<<i)) {
        display_.push_back(' ');
      } else {
        display_.push_back((i<10) ? '0' + i : 'a' + i - 10);
      }
    }
    return display_string(display_.c_str(), STATE(run_test_mode));
  }

  Action prod_mode() {
    LED_GREEN_Pin::set(0);
    LED_RED_Pin::set(1);
    if (!SW1_Pin::get()) {
      return call_immediately(STATE(test_phase));
    }
    return sleep_and_call(&timer_, MSEC_TO_NSEC(1), STATE(prod_mode));
  }

 private:
  Action display_string(const char* text, Callback s) {
    size_t len = strlen(text);
    return write_repeated(&selectHelper_, fd_, text, len, s);
  }

  unsigned buttonState_{0};
  string display_;
  std::vector<const Gpio*> io_;
  std::vector<QuiesceDebouncer> debs_;
  int fd_;  // for display
  StateFlowSelectHelper selectHelper_{this};
  StateFlowTimer timer_{this};
} g_test_flow;

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[]) {
  g_test_flow.init();
  setblink(0);
  while (1) {
    usleep(3000000);
    resetblink(1);
    usleep(50000);
    resetblink(0);
  }
  return 0;
}
