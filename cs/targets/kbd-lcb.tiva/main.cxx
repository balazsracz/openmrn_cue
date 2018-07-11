

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "executor/StateFlow.hxx"
#include "openlcb/SimpleStack.hxx"
#include "openlcb/PolledProducer.hxx"
#include "openlcb/SimpleNodeInfoMockUserFile.hxx"
#include "hardware.hxx"
#include "os/os.h"
#include "true_text.h"
#include "utils/Debouncer.hxx"
#include "utils/StringPrintf.hxx"
#include "utils/blinker.h"


constexpr openlcb::NodeID NODE_ID = UINT64_C(0x0501010114E0);

openlcb::SimpleCanStack stack(NODE_ID);

openlcb::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char *const openlcb::SNIP_DYNAMIC_FILENAME = openlcb::MockSNIPUserFile::snip_user_file_path;

uint64_t EVENTID_BASE = NODE_ID << 16;
using openlcb::GPIOBit;

openlcb::PolledProducer<QuiesceDebouncer, GPIOBit> producer_0(QuiesceDebouncer::Options(3), stack.node(), EVENTID_BASE + 1, EVENTID_BASE + 0, BUT0_Pin::instance());
openlcb::PolledProducer<QuiesceDebouncer, GPIOBit> producer_1(QuiesceDebouncer::Options(3), stack.node(), EVENTID_BASE + 3, EVENTID_BASE + 2, BUT1_Pin::instance());
openlcb::PolledProducer<QuiesceDebouncer, GPIOBit> producer_2(QuiesceDebouncer::Options(3), stack.node(), EVENTID_BASE + 5, EVENTID_BASE + 4, BUT2_Pin::instance());
openlcb::PolledProducer<QuiesceDebouncer, GPIOBit> producer_3(QuiesceDebouncer::Options(3), stack.node(), EVENTID_BASE + 7, EVENTID_BASE + 6, BUT3_Pin::instance());

openlcb::RefreshLoop loop(stack.node(),
                          {&producer_0, &producer_1,
                           &producer_2, &producer_3});

constexpr unsigned NUM_BUTTONS = 4;

uint32_t button_bits_store;
openlcb::BitRangeEventPC button_bits(stack.node(), EVENTID_BASE, &button_bits_store, NUM_BUTTONS);


string d(TEXT_TYPE);

string e("\b\b\b765\n");
int g_downcounter = 15;
bool g_done = false;

class TestFlow : public StateFlowBase {
 public:
  TestFlow() : StateFlowBase(stack.service()) {}

  void init() {
    kbdFd_ = ::open("/dev/kbd", O_RDWR);
    HASSERT(kbdFd_ >= 0);
  }

  void go() {
    g_downcounter = 15;
    start_flow(STATE(countdown));
  }

  Action countdown() {
    if (!SW1_Pin::get()) {
      g_downcounter = 100;
      return exit();
    }
    if (!--g_downcounter) {
      return call_immediately(STATE(go_type));
    } else {
      return sleep_and_call(&timer_, SEC_TO_NSEC(1), STATE(countdown));
    }
  }

  Action go_type() {
    return write_repeated(&selectHelper_, kbdFd_, d.data(), d.size(),
                          STATE(done));
  }

  Action done() {
    g_done = true;
    return exit();
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
} g_type_flow;

string g_countdown("3...2...1...");
string g_go(TEXT_GO);


class CommandFlow : public StateFlowBase {
 public:
  CommandFlow() : StateFlowBase(stack.service()) {
    start_flow(STATE(wait_for_test));
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
    lastTime_.clear();
    lastTime_.resize(NUM_BUTTONS);
    LED_GREEN_Pin::set(1);
    LED_RED_Pin::set(0);
    buttonState_ = 0;
    return display_string("\n\nTest mode.\n", STATE(run_test_mode));
  }

  Action run_test_mode() {
    if (!SW2_Pin::get()) {
      return call_immediately(STATE(prod_mode));
    }
    for (unsigned i = 0; i < NUM_BUTTONS; ++i) {
      bool st = button_bits_store & (1<<i);
      bool previous_st = buttonState_ & (1 << i);
      if (st != previous_st) {
        random_ ^= ((lastTime_[i] >> 3) & 1) << nextBit_++;
        if (nextBit_ >= 32) nextBit_ = 0;
        // we just saw a change.
        buttonState_ = buttonState_ & ~(1 << i);
        if (st) {
          maxState_ |= (1 << i);
          buttonState_ = buttonState_ | (1 << i);
        }
        return display_state(STATE(run_test_mode));
      } else {
        lastTime_[i]++;
      }
    }
    return sleep_and_call(&timer_, MSEC_TO_NSEC(1), STATE(run_test_mode));
  }

  Action display_state(Callback c) {
    display_.clear();
    display_.reserve(NUM_BUTTONS * 2 + 10);
    display_.push_back('\r');
    for (unsigned i = 0; i < NUM_BUTTONS; ++i) {
      if (buttonState_ & (1 << i)) {
        display_.push_back((i < 10) ? '0' + i : 'a' + i - 10);
      } else {
        display_.push_back(' ');
      }
    }
    display_ += "  [";
    for (unsigned i = 0; i < NUM_BUTTONS; ++i) {
      if (maxState_ & (1 << i)) {
        display_.push_back((i < 10) ? '0' + i : 'a' + i - 10);
      } else {
        display_.push_back(' ');
      }
    }
    display_ += "]";
    return display_string(display_.c_str(), c);
  }

  Action prod_mode() {
    LED_GREEN_Pin::set(0);
    LED_RED_Pin::set(1);
    ctOffset_ = 0;
    return display_string("Armed.\nWait for the countdown to zero...\n",
                          STATE(countdown));
  }

  Action do_abort() {
    return display_string("\n\nAborted.\n\n", STATE(test_phase));
  }

  Action countdown() {
    if (ctOffset_ >= g_countdown.size()) {
      return display_string(TEXT_GO, STATE(activate));
    }
    return write_repeated(&selectHelper_, fd_,
                          g_countdown.data() + (ctOffset_++), 1,
                          STATE(countdown_wait));
  }

  Action countdown_wait() {
    if (!SW1_Pin::get()) {
      return do_abort();
    }
    return sleep_and_call(&timer_, MSEC_TO_NSEC(300), STATE(countdown));
  }

  Action activate() {
    LED_GREEN_Pin::set(1);
    LED_RED_Pin::set(1);
    random_ ^= random_ >> 16;
    random_ ^= random_ >> 8;
    random_ ^= random_ >> 4;
    random_ ^= random_ >> 2;
    activeButton_ = random_ % NUM_BUTTONS;
    buttonState_ = 0;
    return sleep_and_call(&timer_, MSEC_TO_NSEC(1), STATE(run_active_mode));
  }

  Action run_active_mode() {
    if (!SW1_Pin::get()) {
      return do_abort();
    }
    for (unsigned i = 0; i < NUM_BUTTONS; ++i) {
      bool st = button_bits_store & (1<<i);
      if (st && ((buttonState_ & (1<<i)) == 0)) {
        buttonState_ = buttonState_ | (1 << i);
        if (i == activeButton_) {
          trigger();
        }
        if ((maxState_ & (~buttonState_)) == 0) {
          return display_state(STATE(triggered));
        }
        return display_state(STATE(run_active_mode));
      }
    }
    return sleep_and_call(&timer_, MSEC_TO_NSEC(1), STATE(run_active_mode));
  }

  void trigger() { g_type_flow.go(); }

  Action triggered() {
    return display_string("\n\nTriggered.\n", STATE(countdown_trigger));
  }

  Action countdown_trigger() {
    if (!SW1_Pin::get()) {
      return do_abort();
    }
    if (g_done) {
      return sleep_and_call(&timer_, SEC_TO_NSEC(15), STATE(print_done));
    }
    display_.clear();
    display_.push_back('\r');
    display_ = StringPrintf("\r %3d ", g_downcounter);
    for (int i = 0; i < 16; ++i) {
      if (i < 17 - g_downcounter) {
        display_.push_back('=');
      } else {
        display_.push_back(' ');
      }
    }
    display_.push_back('>');
    return display_string(display_.c_str(), STATE(wait_triggered));
  }

  Action wait_triggered() {
    return sleep_and_call(&timer_, MSEC_TO_NSEC(50), STATE(countdown_trigger));
  }

  Action print_done() {
    return display_string(TEXT_DONE, STATE(done));
  }

  Action done() {
    LED_RED_Pin::set(0);
    LED_GREEN_Pin::set(0);
    return exit();
  }

 private:
  Action display_string(const char* text, Callback s) {
    size_t len = strlen(text);
    return write_repeated(&selectHelper_, fd_, text, len, s);
  }

  std::vector<unsigned> lastTime_;
  unsigned random_{0};
  unsigned nextBit_{0};
  unsigned buttonState_{0};
  unsigned maxState_{0};
  unsigned ctOffset_{0};
  unsigned activeButton_{1};
  string display_;
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
  stack.add_can_port_select("/dev/can0");
  g_test_flow.init();
  g_type_flow.init();
  stack.start_executor_thread("thread.main", 0, 2000);
  setblink(0);
  while (1) {
    usleep(3000000);
    resetblink(1);
    usleep(50000);
    resetblink(0);
  }
  return 0;
}
