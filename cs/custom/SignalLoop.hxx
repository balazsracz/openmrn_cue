/** \copyright
 * Copyright (c) 2014, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file SignalLoop.hxx
 *
 * Maintains the state of external UART signals, listens to events for their
 * state changes and handles update loop to them.
 *
 * @author Balazs Racz
 * @date 19 Jul 2014
 */

#ifndef _BRACZ_CUSTOM_SIGNALLOOP_HXX_
#define _BRACZ_CUSTOM_SIGNALLOOP_HXX_

#include "utils/Singleton.hxx"
#include "utils/BusMaster.hxx"
#include "openlcb/EventHandlerTemplates.hxx"
#include "custom/SignalPacket.hxx"

namespace bracz_custom {

using SignalBus = Bus<SignalPacket>;

class SignalLoopInterface : public Singleton<SignalLoopInterface> {
 public:
  /** Turns off the signal looping. This is used for temporarily gaining fulla
   * ccess to the bus, in order to send reboot and bootloader packets in a
   * specific sequence. */
  virtual void disable_loop() = 0;
  /** Turns on signal looping. This is the default state at reset. */
  virtual void enable_loop() = 0;

  /// @return the bus master responsible for the scheduling of activities.
  virtual SignalBus::Master* get_bus_master() = 0;
  
  bool isLoopRunning_ = true;
};

enum SignalPriorities {
  // When an aspect needs to be modified due to an event.
  LIVE_UPDATE,
  // Polling button states.
  BUTTON_POLL,
  // Background refreshes of displays.
  DISPLAY_REFRESH,
  // Background refreshes of aspects.
  ASPECT_REFRESH,
  NUM_PRIORITIES
};

/// Defines the policy of bandwidth sharing between the different priority
/// levels. This array goes into { \link ScheduledQueue }. Tokens go through
/// the priority order. A value of 0.8 means that a token at this level gets
/// caught 80% of the time, and passed to the next level 20% of the time. A
/// value of 1 means strict priority order, i.e., if this level is not empty,
/// then no token will pass through to the next level.
static constexpr Fixed16 SIGNAL_PRIORITIES[SignalPriorities::NUM_PRIORITIES] = {
    {Fixed16::FROM_DOUBLE, 0.8},  // live update
    {Fixed16::FROM_DOUBLE, 0.8},  // button poll
    {Fixed16::FROM_DOUBLE, 0.8},  // display refresh
    {1, 0},                       // aspect refresh
};

class SignalLoop : public StateFlowBase,
                   private openlcb::ByteRangeEventC,
                   public SignalLoopInterface,
                   private Atomic,
                   private SignalBus::Activity {
 public:

  
  
  /** We wait this long between two refresh cycles or an interactive update and
   * a refresh cycle. */
  static const int REFRESH_DELAY_MSEC = 700;

  SignalLoop(SignalPacketBaseInterface* bus, openlcb::Node* node,
             uint64_t event_base, int num_signals)
      : StateFlowBase(node->iface()),
        ByteRangeEventC(node, event_base, backingStore_ = static_cast<uint8_t*>(
                                              malloc(num_signals * 2)),
                        num_signals * 2),
        bus_(bus),
        numSignals_(num_signals),
        nextSignal_(0),
        go_sleep_(0),
        waiting_(0),
        paused_(0),
        timer_(this),
        busMaster_(node->iface(), bus, /*idle=*/this, 3)
  {
    memset(backingStore_, 0, num_signals * 2);
    backingStore_[0] = 255;
    //reset_flow(STATE(refresh));
    // Sets all signals to ESTOP.
    auto* b = bus->alloc();
    b->set_done(n_.reset(this));
    send_update(b, 0, 0);
    lastUpdateTime_ = os_get_time_monotonic();
    busMaster_.set_policy((unsigned)SignalPriorities::NUM_PRIORITIES,
                          SIGNAL_PRIORITIES);
    busMaster_.schedule_activity(this, SignalPriorities::ASPECT_REFRESH);
  }

  ~SignalLoop() { free(backingStore_); }

  /// Fills in a buffer for a signal update packet.
  void prep_update_packet(Buffer<SignalPacket>* b, uint8_t address,
                          uint8_t aspect) {
    auto& s = b->data()->payload_;
    s.clear();
    s.push_back(address);
    s.push_back(3);  // len
    s.push_back(3 /*SCMD_ASPECT*/);
    s.push_back(aspect);
  }

  void send_update(Buffer<SignalPacket>* b, uint8_t address, uint8_t aspect) {
    prep_update_packet(b, address, aspect);
    bus_->send(b);
  }

  Action refresh() {
    lastUpdateTime_ += MSEC_TO_NSEC(REFRESH_DELAY_MSEC);
    long long next_refresh_time = lastUpdateTime_ - os_get_time_monotonic();
    waiting_ = 1;
    nextSignal_ = 0;
    return sleep_and_call(&timer_, next_refresh_time, STATE(start_refresh));
  }

  Action start_refresh() {
    waiting_ = 0;
    lastUpdateTime_ = os_get_time_monotonic();
    // Skips signals for which we don't have an address. This might mean that
    // nothing is sent out as part of a refresh.
    while (nextSignal_ < numSignals_ &&
           (!backingStore_[nextSignal_ << 1])) {
      ++nextSignal_;
    }
    if (paused_ || go_sleep_ || (nextSignal_ >= numSignals_)) {
      go_sleep_ = 0;
      return call_immediately(STATE(refresh));
    }
    return allocate_and_call(bus_, STATE(fill_packet));
  }

  void fill_packet(SignalBus::Packet *packet) override {
    // Skips signals for which we don't have an address. Slot zero is always
    // used even if address is 0.
    while (nextSignal_ < numSignals_ &&
           (nextSignal_ > 0 && !backingStore_[nextSignal_ << 1])) {
      ++nextSignal_;
    }
    if (nextSignal_ >= numSignals_) {
      nextSignal_ = 0;
    }
    prep_update_packet(packet, backingStore_[nextSignal_ << 1],
                       backingStore_[(nextSignal_ << 1) + 1]);
    busMaster_.schedule_activity(this, SignalPriorities::ASPECT_REFRESH);
  }
  
  Action fill_packet() {
    auto* b = get_allocation_result(bus_);
    b->set_done(n_.reset(this));
    send_update(b, backingStore_[nextSignal_ << 1],
                backingStore_[(nextSignal_ << 1) + 1]);
    nextSignal_++;
    return wait_and_call(STATE(start_refresh));
  }

  /// Called by ByteRangeEventC when an event changes one of the entries in the
  /// backing store.
  void notify_changed(unsigned offset) override {
    auto* b = bus_->alloc();  // sync alloc -- not very nice.
    send_update(b, backingStore_[offset & ~1], backingStore_[offset | 1]);
    lastUpdateTime_ = os_get_time_monotonic();
    go_sleep_ = 1;
  }

  void enable_loop() OVERRIDE {
    paused_ = 0;
    isLoopRunning_ = true;
    busMaster_.resume();
  }

  void disable_loop() OVERRIDE {
    paused_ = 1;
    isLoopRunning_ = false;
    busMaster_.pause();
  }

  SignalBus::Master* get_bus_master() override {
    return &busMaster_;
  }

 private:
  long long lastUpdateTime_;
  SignalPacketBaseInterface* bus_;
  uint8_t* backingStore_;

  unsigned numSignals_ : 8;
  unsigned nextSignal_ : 8;
  unsigned go_sleep_ : 1;  // 1 if there was an update from the changed callback
  unsigned waiting_ : 1;  // 1 during sleep until next refresh
  unsigned paused_ : 1;

  BarrierNotifiable n_;
  StateFlowTimer timer_;

  SignalBus::Master busMaster_;
};

}  // namespace bracz_custom

#endif  // _BRACZ_CUSTOM_SIGNALLOOP_HXX_
