#include <stdio.h>
#include <unistd.h>

#include "mbed.h"
#include "os/os.h"
#include "utils/Hub.hxx"
#include "utils/HubDevice.hxx"
#include "executor/Executor.hxx"
#include "can_frame.h"
#include "nmranet_config.h"

#include "openlcb/IfCan.hxx"
#include "openlcb/If.hxx"
#include "openlcb/AliasAllocator.hxx"
#include "openlcb/EventService.hxx"
#include "openlcb/EventHandlerTemplates.hxx"
#include "openlcb/DefaultNode.hxx"
#include "freertos_drivers/nxp/11cxx_async_can.hxx"

#include "src/event_range_listener.hxx"
#include "src/mbed_gpio_handlers.hxx"
#include "src/lpc11c_watchdog.hxx"

#include "mbed.h"

// DEFINE_PIPE(gc_can_pipe, 1);

NO_THREAD nt;
Executor<1> g_executor(nt);
Service g_service(&g_executor);
CanHubFlow can_hub0(&g_service);

static const openlcb::NodeID NODE_ID = 0x050101011448ULL;

extern "C" {
const size_t WRITE_FLOW_THREAD_STACK_SIZE = 900;
extern const size_t CAN_TX_BUFFER_SIZE;
extern const size_t CAN_RX_BUFFER_SIZE;
const size_t CAN_RX_BUFFER_SIZE = 1;
const size_t CAN_TX_BUFFER_SIZE = 2;
const size_t main_stack_size = 900;
}

openlcb::IfCan g_if_can(&g_executor, &can_hub0, 1, 3, 2);
static openlcb::AddAliasAllocator _alias_allocator(NODE_ID, &g_if_can);
openlcb::DefaultNode g_node(&g_if_can, NODE_ID);
openlcb::EventService g_event_service(&g_if_can);
WatchDogEventHandler g_watchdog(&g_node, WATCHDOG_EVENT_ID);

static const uint64_t EVENT_ID = 0x0501010114FF2820ULL;
const int main_priority = 0;

extern "C" {

#ifdef __FreeRTOS__ //TARGET_LPC11Cxx
#endif
  
}

extern "C" { void resetblink(uint32_t pattern); }

class LoggingBit : public openlcb::BitEventInterface
{
public:
    LoggingBit(uint64_t event_on, uint64_t event_off, const char* name)
        : BitEventInterface(event_on, event_off), name_(name), state_(false)
    {
    }

    virtual bool GetCurrentState()
    {
        return state_;
    }
    virtual void SetState(bool new_value)
    {
        state_ = new_value;
        //HASSERT(0);
#ifdef __linux__
        LOG(INFO, "bit %s set to %d", name_, state_);
#else
        resetblink(state_ ? 1 : 0);
#endif
    }

    virtual openlcb::Node* node()
    {
        return &g_node;
    }

private:
    const char* name_;
    bool state_;
};


static const uint64_t R_EVENT_ID = 0x0501010114FF2800ULL;


MbedGPIOListener listen_r0(R_EVENT_ID + 0, R_EVENT_ID + 1, P1_8);
MbedGPIOListener listen_r1(R_EVENT_ID + 2, R_EVENT_ID + 3, P0_2);
MbedGPIOListener listen_r2(R_EVENT_ID + 4, R_EVENT_ID + 5, P2_7);
MbedGPIOListener listen_r3(R_EVENT_ID + 6, R_EVENT_ID + 7, P2_8);


class AllGPIOProducers : public StateFlowBase {
 public:
  AllGPIOProducers(uint64_t event_base)
      : StateFlowBase(&g_service),
        exported_bits_(0),
        producer_(&g_node, event_base, &exported_bits_, kPinCount),
        timer_(this) {
    int next = 0;
    AddPort(next++, P2_11);
    AddPort(next++, P0_6);  // TODO: this should  be 0_10 but that's SWCLK
    AddPort(next++, P0_9);
    AddPort(next++, P0_8);
    HASSERT(next == kPinCount);
    InitCounts();
    start_flow(STATE(sleep_for_poll));
  }

 private:
  void AddPort(int num, PinName pin) {
    mask_[num] = gpio_set(pin);
    gpio_t gpio;
    gpio_init(&gpio, pin, PIN_INPUT);
    gpio_mode(&gpio, PullNone);
    memory_[num] = gpio.reg_in;
  }

  void InitCounts() {
    memset(counts_, 0, sizeof(counts_));
    max_count_ = 0;
  }

  Action sleep_for_poll() {
    return sleep_and_call(&timer_, MSEC_TO_NSEC(1), STATE(poll));
  }

  Action poll() {
    for (int i = 0; i < kPinCount; ++i) {
      if ((*memory_[i]) & mask_[i]) {
        counts_[i]++;
      }
    }
    if (++max_count_ == 0xff) {
      // Evaluates the captured data.
      last_read_bits_ = 0;
      for (int i = 0; i < kPinCount; ++i) {
        if (counts_[i] > 128) {
          last_read_bits_ |= (1<<i);
        }
      }
      InitCounts();
      // Schedule callback on the main thread to send off events.
      return call_immediately(STATE(send_off_events));
    }
    return call_immediately(STATE(sleep_for_poll));
  }

  Action send_off_events() {
    for (int i = 0; i < kPinCount; ++i) {
      if (!helper_busy_.is_done()) return wait();
      uint32_t mask =  1<<i;
      bool new_value = (last_read_bits_ & mask) ? true : false;
      producer_.Set(i, new_value, &helper_, helper_busy_.reset(this));
    }
    return call_immediately(STATE(sleep_for_poll));
  }

  static const int kPinCount = 4;

  volatile uint32_t* memory_[kPinCount];
  uint32_t mask_[kPinCount];

  uint8_t counts_[kPinCount];
  uint8_t max_count_;
  uint8_t pending_ {0};

  // Backing store of the range producer.
  uint32_t exported_bits_;
  // Fresh bits that we need to copy to the exported bits.
  uint32_t last_read_bits_;

  openlcb::BitRangeEventPC producer_;

  openlcb::WriteHelper helper_;
  BarrierNotifiable helper_busy_;
  StateFlowTimer timer_;
};

/*class AllGPIOProducers : private Executable, private Atomic {
 public:
  AllGPIOProducers(uint64_t event_base)
      : exported_bits_(0),
        producer_(&g_node, event_base, &exported_bits_, kPinCount),
        timer_(AllGPIOProducers::timer_callback, this, nullptr) {
    int next = 0;
    AddPort(next++, P2_11);
    AddPort(next++, P0_6);  // TODO: this should  be 0_10 but that's SWCLK
    AddPort(next++, P0_9);
    AddPort(next++, P0_8);
    HASSERT(next == kPinCount);
    InitCounts();
    timer_.start(MSEC_TO_NSEC(1));
  }

 private:
  void run() override {
    {
      AtomicHolder h(this);
      pending_ = 0;
    }
    for (int i = 0; i < kPinCount; ++i) {
      if (!helper_busy_.is_done()) return;
      uint32_t mask =  1<<i;
      bool new_value = (last_read_bits_ & mask) ? true : false;
      producer_.Set(i, new_value, &helper_, helper_busy_.reset(this));
    }
  }

  void notify() override {
    {
      AtomicHolder h(this);
      if (!pending_) {
        pending_ = 1;
      } else {
        return;
      }
      g_executor.add(this);
    }
  }

  void AddPort(int num, PinName pin) {
    mask_[num] = gpio_set(pin);
    gpio_t gpio;
    gpio_init(&gpio, pin, PIN_INPUT);
    gpio_mode(&gpio, PullNone);
    memory_[num] = gpio.reg_in;
  }

  void InitCounts() {
    memset(counts_, 0, sizeof(counts_));
    max_count_ = 0;
  }

  static long long timer_callback(void* object, void* unused) {
    AllGPIOProducers* me = static_cast<AllGPIOProducers*>(object);
    for (int i = 0; i < kPinCount; ++i) {
      if ((*me->memory_[i]) & me->mask_[i]) {
        me->counts_[i]++;
      }
    }
    if (++me->max_count_ == 0xff) {
      // Evaluates the captured data.
      taskENTER_CRITICAL();
      me->last_read_bits_ = 0;
      for (int i = 0; i < kPinCount; ++i) {
        if (me->counts_[i] > 128) {
          me->last_read_bits_ |= (1<<i);
        }
      }
      taskEXIT_CRITICAL();
      // Schedule callback on the main thread to send off events.
      me->notify();
      me->InitCounts();
    }
    return OS_TIMER_RESTART;
  }

  static const int kPinCount = 4;

  volatile uint32_t* memory_[kPinCount];
  uint32_t mask_[kPinCount];

  uint8_t counts_[kPinCount];
  uint8_t max_count_;
  uint8_t pending_ {0};

  // Backing store of the range producer.
  uint32_t exported_bits_;
  // Fresh bits that we need to copy to the exported bits.
  uint32_t last_read_bits_;

  openlcb::BitRangeEventPC producer_;
  OSTimer timer_;

  openlcb::WriteHelper helper_;
  BarrierNotifiable helper_busy_;
  };*/

static const uint64_t DET_EVENT_ID = 0x0501010114FF2810ULL;
AllGPIOProducers input_producers_(DET_EVENT_ID);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
#ifdef TARGET_LPC11Cxx
  lpc11cxx::CreateCanDriver(&can_hub0);
#endif
  //BlinkerFlow blinker(&g_node);
    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    openlcb::BitEventConsumer consumer(&logger);
    //g_if_can.add_addressed_message_support();
    openlcb::AliasInfo info;
    // Bootstraps the alias allocation process.
    g_if_can.alias_allocator()->send(g_if_can.alias_allocator()->alloc());
    //openlcb::AddEventHandlerToIf(&g_if_can);
    g_executor.thread_body();
    return 0;
}
