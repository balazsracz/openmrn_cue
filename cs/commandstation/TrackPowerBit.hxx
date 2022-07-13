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

#ifndef _COMMANDSTATION_TRACKPOWERBIT_HXX_
#define _COMMANDSTATION_TRACKPOWERBIT_HXX_

#include "dcc/DccOutput.hxx"
#include "dcc/PacketSource.hxx"
#include "dcc/UpdateLoop.hxx"
#include "openlcb/EventHandlerTemplates.hxx"

namespace commandstation {

/// Controls the track power and emergency stop of a command station in three
/// phases:
///
/// - Regular operation
///
/// - Global Emergency stop: track is powered, but all packets sent are
///   broadcast ESTOP packets.
///
/// - Track power off.
///
/// This is controlled by two LCC event pair consumers, one for track power
/// off/on and another for global ESTOP. Track power off takes precedence.
///
/// A special feature is that transitioning from regular operation to track
/// power off state goes through a short period of Global Emergency Stop
/// state. This ensures that locomotives that have supercap technology also get
/// stopped.
class TrackPowerState {
 public:
  /// @param node openlcb node on which to listen for events.
  /// @param event_eoff_on event that that sets track power on.
  /// @param event_eoff_off event that turns track power off.
  /// @param event_estop_on event that clears global estop condition.
  /// @param event_estop_off event that invokes global estop condition.
  TrackPowerState(openlcb::Node* node, uint64_t event_eoff_on,
                  uint64_t event_eoff_off, uint64_t event_estop_on,
                  uint64_t event_estop_off)
      : node_(node),
        trackPowerBit_(this, event_eoff_on, event_eoff_off),
        estopBit_(this, event_estop_on, event_estop_off) {}

  /// @return the event bit representing the track power bit. The caller must
  /// instantiate a Consumer object on this.
  openlcb::BitEventInterface* get_power_bit() { return &trackPowerBit_; }

  /// @return the event bit representing the global estop bit. The caller must
  /// instantiate a Consumer object on this.
  openlcb::BitEventInterface* get_estop_bit() { return &estopBit_; }

 private:
  /// OpenLCB node for the CS.
  openlcb::Node* node_;

  /// Installs the global estop packet source. Once installed, no locomotive
  /// packets are sent out anymore, only the broadcast ESTOP packets,
  /// alternating DCC and MM.
  /// @param done will be called after 20 estop packets are sent, if not
  /// nullptr.
  void register_source(std::function<void()> done) {
    if (!estopSource_.isRegistered_) {
      estopSource_.isRegistered_ = true;
      packet_processor_add_refresh_source(&estopSource_,
                                          dcc::UpdateLoopBase::ESTOP_PRIORITY);
    }
    if (done) {
      // Starts counting estop packets, and calls done after 20 of them.
      estopSource_.start(done);
    }
  }

  /// Removes the global estop packet source, if both EOff and EStop bits are
  /// in normal operation mode.
  void unregister_source() {
    if (!estopSource_.isRegistered_) {
      return;
    }
    if (estopSource_.isEOff_ == 0 && estopSource_.isEStop_ == 0) {
      packet_processor_remove_refresh_source(&estopSource_);
      estopSource_.isRegistered_ = false;
    }
  }

  /// DCC Packet Source that only produces broadcast ESTOP packets, alternating
  /// for Marklin and DCC.
  class EstopPacketSource : public dcc::NonTrainPacketSource {
   public:
    EstopPacketSource()
        : isRegistered_(0), isEOff_(0), isEStop_(0), nextDcc_(1) {}

    /// Call this function to start sending ESTOP packets and get a callback
    /// when 20 estop packets were sent to the track.
    void start(std::function<void()> done) {
      nextDcc_ = 1;
      numToNotify_ = 20;
      done_ = std::move(done);
    }

    /// @return true if we still have some of the startup ESTOP packets to
    /// send.
    bool counting() { return numToNotify_ > 0; }

   private:
    /// Implementation for the packet source. Called by the packet update loop.
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
    /// True if this packet source is registered in the update loop. Used by
    /// the caller to keep state.
    uint8_t isRegistered_ : 1;
    /// True if track power is controlled to be off. Used by the caller to keep
    /// state.
    uint8_t isEOff_ : 1;
    /// True if global estop is engaged. Used by the caller to keep state.
    uint8_t isEStop_ : 1;

   private:
    /// Whether the next estop packet should be dcc or mm.
    uint8_t nextDcc_ : 1;
    /// How many packets to wait for notify.
    uint8_t numToNotify_ : 6;
    /// When the num to notify expires, we call this.
    std::function<void()> done_{nullptr};
  } estopSource_;

  /// Bit implementation that handles the track power off/on events.
  class TrackPowerBit : public openlcb::BitEventInterface {
   public:
    static constexpr auto REASON = DccOutput::DisableReason::GLOBAL_EOFF;
    static constexpr unsigned UREASON = (unsigned)REASON;

    /// @param parent is the TrackPowerState object.
    /// @param event_on turns track power on
    /// @param event_off turns track power off
    TrackPowerBit(TrackPowerState* parent, uint64_t event_on,
                  uint64_t event_off)
        : BitEventInterface(event_on, event_off), parent_(parent) {}

    openlcb::EventState get_current_state() OVERRIDE {
      auto disable_bits =
          get_dcc_output(DccOutput::TRACK)->get_disable_output_reasons();
      return (disable_bits & UREASON) == 0 ? openlcb::EventState::VALID
                                           : openlcb::EventState::INVALID;
    }

    void set_state(bool new_value) OVERRIDE {
      if (new_value) {
        LOG(WARNING, "enable dcc");
        parent_->estopSource_.isEOff_ = 0;
        parent_->unregister_source();
        get_dcc_output(DccOutput::TRACK)
            ->clear_disable_output_for_reason(REASON);
        get_dcc_output(DccOutput::LCC)->clear_disable_output_for_reason(REASON);
      } else {
        LOG(WARNING, "send EOFF");
        parent_->estopSource_.isEOff_ = 1;
        parent_->register_source(
            std::bind(&TrackPowerBit::estop_expired, this));
      }
    }

    openlcb::Node* node() override { return parent_->node_; }

   private:
    /// Callback when the 20 estop packets are sent out after engaging track
    /// power off.
    void estop_expired() {
      LOG(WARNING, "disable dcc");
      get_dcc_output(DccOutput::TRACK)->disable_output_for_reason(REASON);
      get_dcc_output(DccOutput::LCC)->disable_output_for_reason(REASON);
    }

    TrackPowerState* parent_;
  } trackPowerBit_;

  /// Bit implementation that handles the global estop off/on events.
  class GlobalStopBit : public openlcb::BitEventInterface {
   public:
    /// @param parent is the TrackPowerState object.
    /// @param event_on turns trains on
    /// @param event_off switches to global estop mode
    GlobalStopBit(TrackPowerState* parent, uint64_t event_on,
                  uint64_t event_off)
        : BitEventInterface(event_on, event_off), parent_(parent) {}

    openlcb::Node* node() override { return parent_->node_; }

    openlcb::EventState get_current_state() OVERRIDE {
      if (parent_->estopSource_.isEStop_) {
        return openlcb::EventState::INVALID;
      } else {
        return openlcb::EventState::VALID;
      }
    }

    void set_state(bool new_value) OVERRIDE {
      if (new_value) {
        parent_->estopSource_.isEStop_ = false;
        parent_->unregister_source();
      } else {
        parent_->estopSource_.isEStop_ = true;
        parent_->register_source(nullptr);
      }
    }

   private:
    /// Links to the parent object.
    TrackPowerState* parent_;
  } estopBit_;
};  // class TrackPowerState

}  // namespace commandstation

#endif // _COMMANDSTATION_TRACKPOWERBIT_HXX_
