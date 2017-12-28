/** \copyright
 * Copyright (c) 2016, Balazs Racz
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
 * \file TrackPowerBit.hxx
 *
 * OpenLCB binding to the dcc track power bit.
 *
 * @author Balazs Racz
 * @date 6 Feb 2016
 */

#include "commandstation/dcc_control.hxx"
#include "dcc/PacketSource.hxx"
#include "dcc/UpdateLoop.hxx"
#include "openlcb/EventHandlerTemplates.hxx"

namespace commandstation {

class TrackPowerBit : public openlcb::BitEventInterface {
 public:
  TrackPowerBit(openlcb::Node* node, uint64_t event_on, uint64_t event_off)
      : BitEventInterface(event_on, event_off), node_(node) {}

  openlcb::EventState get_current_state() OVERRIDE {
    return query_dcc() ? openlcb::EventState::VALID
                       : openlcb::EventState::INVALID;
  }
  void set_state(bool new_value) OVERRIDE {
    if (new_value) {
      LOG(WARNING, "enable dcc");
      if (estopSource_.isRegistered_) {
        packet_processor_remove_refresh_source(&estopSource_);
        estopSource_.isRegistered_ = 0;
      }
      enable_dcc();
    } else {
      LOG(WARNING, "send ESTOP");
      if (estopSource_.isRegistered_) {
        return estop_expired();
      } else {
        packet_processor_add_refresh_source(
            &estopSource_, dcc::UpdateLoopBase::ESTOP_PRIORITY);
        estopSource_.start(std::bind(&TrackPowerBit::estop_expired, this));
        estopSource_.isRegistered_ = 1;
      }
    }
  }

  openlcb::Node* node() OVERRIDE { return node_; }

 private:
  void estop_expired() {
    LOG(WARNING, "disable dcc");
    disable_dcc();
  }

  class EstopPacketSource : public dcc::NonTrainPacketSource {
   public:
    EstopPacketSource() : isRegistered_(0) {}

    /// Call this function to start sending ESTOP packets and get a callback
    /// when 20 estop packets were sent to the track.
    void start(std::function<void()> done) {
      nextDcc_ = 1;
      numToNotify_ = 20;
      done_ = std::move(done);
    }

   private:
    void get_next_packet(unsigned code, dcc::Packet* packet) override {
      if (nextDcc_) {
        packet->set_dcc_speed14(dcc::DccShortAddress(0), true, false,
                                dcc::Packet::EMERGENCY_STOP);
      } else {
        packet->start_mm_packet();
        packet->add_mm_address(dcc::MMAddress(0), false);
        packet->add_mm_speed(dcc::Packet::EMERGENCY_STOP);
      }
      nextDcc_ ^= 1;
      if (numToNotify_) {
        if (--numToNotify_ == 0 && done_) {
          done_();
          done_ = nullptr;
        }
      }
    }

   public:
    /// Used by the caller to keep state.
    uint8_t isRegistered_ : 1;

   private:
    /// Whether the next estop packet should be dcc or mm.
    uint8_t nextDcc_ : 1;
    /// How many packets to wait for notify.
    uint8_t numToNotify_ : 6;
    /// When the num to notify expires, we call this.
    std::function<void()> done_{nullptr};
  } estopSource_;

  openlcb::Node* node_;
};

}  // namespace commandstation
