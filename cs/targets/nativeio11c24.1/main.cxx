#include <stdio.h>
#include <unistd.h>

#include "mbed.h"
#include "os/os.h"
#include "utils/pipe.hxx"
#include "executor/executor.hxx"
#include "nmranet_can.h"
#include "nmranet_config.h"

#include "nmranet/IfCan.hxx"
#include "nmranet/NMRAnetIf.hxx"
#include "nmranet/AliasAllocator.hxx"
#include "nmranet/GlobalEventHandler.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/NMRAnetAsyncEventHandler.hxx"
#include "nmranet/DefaultNode.hxx"
#include "freertos_drivers/nxp/11cxx_async_can.hxx"

#include "src/event_range_listener.hxx"
#include "src/mbed_gpio_handlers.hxx"
#include "src/lpc11c_watchdog.hxx"

#include "mbed.h"

// DEFINE_PIPE(gc_can_pipe, 1);

Executor g_executor;

DEFINE_PIPE(can_pipe, &g_executor, sizeof(struct can_frame));

static const nmranet::NodeID NODE_ID = 0x050101011448ULL;

extern "C" {
const size_t WRITE_FLOW_THREAD_STACK_SIZE = 900;
extern const size_t CAN_TX_BUFFER_SIZE;
extern const size_t CAN_RX_BUFFER_SIZE;
const size_t CAN_RX_BUFFER_SIZE = 1;
const size_t CAN_TX_BUFFER_SIZE = 2;
const size_t main_stack_size = 900;
}

nmranet::AsyncIfCan g_if_can(&g_executor, &can_pipe, 3, 3, 2, 1, 1);
nmranet::DefaultNode g_node(&g_if_can, NODE_ID);
nmranet::GlobalEventFlow g_event_flow(&g_executor, 4);
WatchDogEventHandler g_watchdog(&g_node, WATCHDOG_EVENT_ID);

static const uint64_t EVENT_ID = 0x0501010114FF2820ULL;
const int main_priority = 0;

extern "C" {

#ifdef __FreeRTOS__ //TARGET_LPC11Cxx
#endif
  
}

Executor* DefaultWriteFlowExecutor() {
    return &g_executor;
}

extern "C" { void resetblink(uint32_t pattern); }

class LoggingBit : public nmranet::BitEventInterface
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

    virtual nmranet::Node* node()
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

class AllGPIOProducers : private Executable, private Notifiable {
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
  virtual void Run() {
    for (int i = 0; i < kPinCount; ++i) {
      if (!helper_busy_.HasBeenNotified()) return;
      uint32_t mask =  1<<i;
      bool new_value = (last_read_bits_ & mask) ? true : false;
      producer_.Set(i, new_value, &helper_, helper_busy_.NewCallback(this));
    }
  }

  virtual void Notify() {
    AtomicHolder h(&g_executor);
    if (!g_executor.IsMaybePending(this)) {
      g_executor.Add(this);
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

  static long long timer_callback(void* object, void* /*unused*/) {
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
      me->Notify();
      me->InitCounts();
    }
    return OS_TIMER_RESTART;
  }

  static const int kPinCount = 4;

  volatile uint32_t* memory_[kPinCount];
  uint32_t mask_[kPinCount];

  uint8_t counts_[kPinCount];
  uint8_t max_count_;

  // Backing store of the range producer.
  uint32_t exported_bits_;
  // Fresh bits that we need to copy to the exported bits.
  uint32_t last_read_bits_;

  nmranet::BitRangeEventPC producer_;
  OSTimer timer_;

  nmranet::WriteHelper helper_;
  ProxyNotifiable helper_busy_;
};

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
  lpc11cxx::CreateCanDriver(&can_pipe);
#endif
  //BlinkerFlow blinker(&g_node);
    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    nmranet::BitEventConsumer consumer(&logger);
    g_if_can.set_alias_allocator(
        new nmranet::AsyncAliasAllocator(NODE_ID, &g_if_can));
    nmranet::AliasInfo info;
    g_if_can.alias_allocator()->empty_aliases()->Release(&info);
    nmranet::AddEventHandlerToIf(&g_if_can);
    g_executor.ThreadBody();
    return 0;
}
