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
#include "nmranet/EventHandlerTemplates.hxx"
#include "custom/SignalPacket.hxx"

namespace bracz_custom {

class SignalLoopInterface : public Singleton<SignalLoopInterface> {
 public:
  /** Turns off the signal looping. This is used for temporarily gaining fulla
   * ccess to the bus, in order to send reboot and bootloader packets in a
   * specific sequence. */
  virtual void disable_loop() = 0;
  /** Turns on signal looping. This is the default state at reset. */
  virtual void enable_loop() = 0;
};


class SignalLoop : public StateFlowBase,
                   private openlcb::ByteRangeEventC,
                   public SignalLoopInterface,
                   private Atomic {
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
        timer_(this)
  {
    memset(backingStore_, 0, num_signals * 2);
    reset_flow(STATE(refresh));
    // Sets all signals to ESTOP.
    auto* b = bus->alloc();
    b->set_done(n_.reset(this));
    send_update(b, 0, 0);
    lastUpdateTime_ = os_get_time_monotonic();
  }

  ~SignalLoop() { free(backingStore_); }

  void send_update(Buffer<string>* b, uint8_t address, uint8_t aspect) {
    auto& s = *b->data();
    s.clear();
    s.push_back(address);
    s.push_back(3);  // len
    s.push_back(3 /*SCMD_ASPECT*/);
    s.push_back(aspect);
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
    // Skips signals for which we don't have an address. Slot zero is always
    // used even if address is 0.
    while (nextSignal_ < numSignals_ &&
           (nextSignal_ > 0 && !backingStore_[nextSignal_ << 1])) {
      ++nextSignal_;
    }
    if (paused_ || go_sleep_ || (nextSignal_ >= numSignals_)) {
      go_sleep_ = 0;
      return call_immediately(STATE(refresh));
    }
    return allocate_and_call(bus_, STATE(fill_packet));
  }

  Action fill_packet() {
    auto* b = get_allocation_result(bus_);
    b->set_done(n_.reset(this));
    send_update(b, backingStore_[nextSignal_ << 1],
                backingStore_[(nextSignal_ << 1) + 1]);
    nextSignal_++;
    return wait_and_call(STATE(start_refresh));
  }

  void notify_changed(unsigned offset) OVERRIDE {
    auto* b = bus_->alloc();  // sync alloc -- not very nice.
    send_update(b, backingStore_[offset & ~1], backingStore_[offset | 1]);
    lastUpdateTime_ = os_get_time_monotonic();
    go_sleep_ = 1;
  }

  void enable_loop() OVERRIDE {
    paused_ = 0;
  }

  void disable_loop() OVERRIDE {
    paused_ = 1;
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
};

}  // namespace bracz_custom

#endif  // _BRACZ_CUSTOM_SIGNALLOOP_HXX_
