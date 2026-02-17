/** \copyright
 * Copyright (c) 2015, Balazs Racz
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
 * \file RailcomBroadcastFlow.hxx
 *
 * Listens to messages on the railcom hub, and decodes railcom ID1 and ID2
 * messages coming in channel1 to determine what DCC address decoders are
 * present in the current block. Implemented for multiple channels
 * directly. Exposes the found locomotives using a 16-bit event encoding per
 * channel.
 *
 * @author Balazs Racz
 * @date 13 Dec 2015
 */

#ifndef _BRACZ_CUSTOM_RAILCOMBROADCASTFLOW_HXX_
#define _BRACZ_CUSTOM_RAILCOMBROADCASTFLOW_HXX_

#include "dcc/RailcomHub.hxx"
#include "dcc/RailcomBroadcastDecoder.hxx"
#include "dcc/Address.hxx"
#include "dcc/Defs.hxx"
#include "openlcb/EventHandler.hxx"
#include "openlcb/EventHandlerTemplates.hxx"

#include <map>
#include "dcc/packet.h"
#include "os/OS.hxx"

/// Listens to messages on the railcom hub, and decodes railcom ID1 and ID2
/// messages coming in channel1 to determine what DCC address decoders are
/// present in the current block. Implemented for multiple channels
/// directly. Exposes the found locomotives using a 16-bit event encoding per
/// channel.
class RailcomBroadcastFlow : public dcc::RailcomHubPort,
                             openlcb::SimpleEventHandler {
 public:
  RailcomBroadcastFlow(dcc::RailcomHubFlow* parent, openlcb::Node* node,
                       dcc::RailcomHubPortInterface* occupancy_port,
                       dcc::RailcomHubPortInterface* overcurrent_port,
                       dcc::RailcomHubPortInterface* debug_port,
                       unsigned channel_count)
      : dcc::RailcomHubPort(parent->service()),
        parent_(parent),
        node_(node),
        occupancyPort_(occupancy_port),
        overcurrentPort_(overcurrent_port),
        debugPort_(debug_port),
        size_(channel_count),
        channels_(new dcc::RailcomBroadcastDecoder[channel_count]) {
    parent_->register_port(this);
    // Registers event handler for range.
    uint64_t event_base = this->event_base();
    unsigned mask =
        openlcb::EventRegistry::align_mask(&event_base, 65536 * size_);
    openlcb::EventRegistry::instance()->register_handler(
        EventRegistryEntry(this, event_base), mask);
    
  }

  ~RailcomBroadcastFlow() {
    openlcb::EventRegistry::instance()->unregister_handler(this);
    parent_->unregister_port(this);
    delete[] channels_;
  }

  /// @return the currently valid DCC address, or zero if no valid address
  /// right now.
  /// @param channel which channel (0 to channel_count)
  uint16_t current_address(unsigned ch)
  {
    return channels_[ch].current_address();
  }

  /// Called when a DCC packet is sent to the track.
  /// @param packet The DCC packet that was sent.
  void handle_dcc_packet(const DCCPacket* packet) {
      uint16_t address = dcc_to_address(packet->payload[0], packet->payload[1]);
      if (address == 0xFFFF) return; // Not a valid/supported mobile address

      OSMutexLock l(&lock_);
      // Address-major key: (Address << 16) | Channel
      uint32_t start_key = (uint32_t(address) << 16);

      // Use lower_bound to find the first entry for this address
      auto it = trackers_.lower_bound(start_key);

      // Iterate while the address matches
      while (it != trackers_.end() && ((it->first >> 16) == address)) {
          if (it->second.report_loco_addressed()) {
              pending_deletions_ = true;
          }
          ++it;
      }
  }
  
  Action entry() override {
    auto channel = message()->data()->channel;
    if (channel == 0xff) {
      // occupancy port
      uint32_t new_values = message()->data()->ch1Data[0];
      for (unsigned i = 0; i < size_; ++i) {
        channels_[i].set_occupancy(new_values & (1 << i));
        auto& decoder = channels_[i];
        if (decoder.current_address() != decoder.lastAddress_) {
          // We need to send a fake railcom packet to ourselves to allow the
          // channel empty events to be generated.
          auto* b = alloc();
          b->data()->reset(0, 0xFF00);
          b->data()->channel = i;
          send(b);
        }
      }
      if (occupancyPort_) {
        occupancyPort_->send(transfer_message());
      } else {
        release();
      }
      return exit();
    }
    if (channel == 0xfe) {
      if (overcurrentPort_) {
        overcurrentPort_->send(transfer_message());
      } else {
        release();
      }
      return exit();
    }

    // Start processing ch2
    return call_immediately(STATE(process_ch2));
  }

  Action check_timeouts() {
      OSMutexLock l(&lock_);
      if (!pending_deletions_) return call(STATE(process_ch1));

      // Find tracker with count 0
      auto it = trackers_.lower_bound(current_timeout_key_); // Start from saved key
      while (it != trackers_.end()) {
          if (it->second.count == 0) {
              current_timeout_key_ = it->first;
              trackers_.erase(it); // Remove from map
              return allocate_and_call(node_->iface()->global_message_write_flow(), STATE(send_timeout_event));
          }
          ++it;
      }
      // Reached end, clear flag and reset key
      pending_deletions_ = false;
      current_timeout_key_ = 0;
      return call(STATE(process_ch1));
  }

  Action send_timeout_event() {
      auto* b = get_allocation_result(node_->iface()->global_message_write_flow());
      uint16_t addr = current_timeout_key_ >> 16;
      unsigned ch = current_timeout_key_ & 0xFFFF;

      b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT, node_->node_id(),
                     openlcb::eventid_to_buffer(address_to_eventid(
                         ch, addr, false))); // false = exit
      b->set_done(n_.reset(this));
      node_->iface()->global_message_write_flow()->send(b);
      // Loop back to check for more timeouts
      return wait_and_call(STATE(check_timeouts));
  }

  Action process_ch2() {
      auto& msg = *message()->data();
      if (msg.channel >= size_) {
          return call(STATE(check_timeouts));
      }

      // Check Ch2 validity
      if (msg.ch2Size > 0) {
          bool valid = true;
          for (unsigned i = 0; i < msg.ch2Size; ++i) {
             if (dcc::railcom_decode[msg.ch2Data[i]] == dcc::RailcomDefs::INV) {
                 valid = false;
                 break;
             }
          }
          if (valid) {
              // Extract address from dccAddress
              // Note: dccAddress is the address of the packet that triggered the response.
              uint16_t dccAddr = msg.dccAddress;
              uint16_t addr = 0;
              // Decode address same as in handle_dcc_packet
              uint8_t b0 = dccAddr >> 8;
              if ((b0 & 0xC0) == 0xC0) {
                 addr = dccAddr & 0x3FFF;
              } else if (b0 <= 127 && b0 != 0) {
                 addr = b0;
              } else {
                 valid = false; // Not a mobile address we support
              }

              if (valid) {
                  OSMutexLock l(&lock_);
                  uint32_t key = (msg.channel << 16) | addr;
                  bool is_new = false;
                  auto it = trackers_.find(key);
                  if (it == trackers_.end()) {
                      is_new = true;
                      trackers_[key].report_loco_seen();
                  } else {
                      it->second.report_loco_seen();
                  }

                  if (is_new) {
                      current_timeout_key_ = key; // Reuse this member for new loco key
                      return allocate_and_call(node_->iface()->global_message_write_flow(), STATE(send_ch2_event));
                  }
              }
          }
      }

      return call(STATE(check_timeouts));
  }

  Action send_ch2_event() {
      auto* b = get_allocation_result(node_->iface()->global_message_write_flow());
      unsigned ch = current_timeout_key_ >> 16;
      uint16_t addr = current_timeout_key_ & 0xFFFF;

      b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT, node_->node_id(),
                     openlcb::eventid_to_buffer(address_to_eventid(
                         ch, addr, true))); // true = entry
      b->set_done(n_.reset(this));
      node_->iface()->global_message_write_flow()->send(b);

      return wait_and_call(STATE(check_timeouts));
  }

  Action process_ch1() {
    auto channel = message()->data()->channel;
    if (channel >= size_ ||
        !channels_[channel].process_packet(*message()->data())) {
      if (debugPort_) {
        debugPort_->send(transfer_message());
      } else {
        release();
      }
      return exit();
    }
    auto& decoder = channels_[channel];
    if (decoder.current_address() == decoder.lastAddress_) {
      return release_and_exit();
    }
    // Now: the visible address has changed. Send event reports.
    return allocate_and_call(node_->iface()->global_message_write_flow(),
                             STATE(send_invalid));
  }

  Action send_invalid() {
    auto channel = message()->data()->channel;
    auto& decoder = channels_[channel];
    auto* b =
        get_allocation_result(node_->iface()->global_message_write_flow());
    b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT, node_->node_id(),
                     openlcb::eventid_to_buffer(address_to_eventid(
                         channel, decoder.lastAddress_, false)));
    b->set_done(n_.reset(this));
    node_->iface()->global_message_write_flow()->send(b);
    return wait_and_call(STATE(allocate_for_event));
  }

  Action allocate_for_event() {
    return allocate_and_call(node_->iface()->global_message_write_flow(),
                             STATE(send_event));
  }

  Action send_event() {
    auto channel = message()->data()->channel;
    auto& decoder = channels_[channel];
    auto* b =
        get_allocation_result(node_->iface()->global_message_write_flow());
    b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT, node_->node_id(),
                     openlcb::eventid_to_buffer(address_to_eventid(
                         channel, decoder.current_address(), true)));
    b->set_done(n_.reset(this));
    node_->iface()->global_message_write_flow()->send(b);
    decoder.lastAddress_ = decoder.current_address();
    release();
    return wait_and_call(STATE(finish));
  }

  Action finish() { return exit(); }

 private:
  static constexpr uint64_t FEEDBACK_EVENTID_BASE = (0x0680ULL << 48);
  /// Computes the event ID for a report to send.
  ///
  /// @param channel 0-15, the channel ID for a multichannel detector to use.
  /// @param address DCC address, the concatenation of ID1 and ID2 values.
  /// @param entry true for entry, false for exit.
  ///
  /// Converts a DCC address to an Event ID for RailCom reporting.
  ///
  /// @param channel 0-15, the channel ID for a multichannel detector to use.
  /// @param address 14-bit DCC address (0-10239).
  /// @param entry true for entry, false for exit.
  ///
  /// @return event ID to send as event report.
  uint64_t address_to_eventid(unsigned channel, uint16_t address, bool entry) {
    uint64_t ret = event_base();
    ret |= uint64_t(channel & 0xf) << 16;
    // Direction unknown (0xC000 for entry, 0 for exit)
    uint16_t val = entry ? 0xC000 : 0;

    // Check if it's a short address (0-127) or long/extended address
    if (address <= 127) {
        // Short address
        val |= (address & 0x7F) | (dcc::Defs::ADR_MOBILE_SHORT << 8);
    } else {
        // Long address (or potentially Consist, but handled as Long here for simplicity if not distinguished upstream)
        // Ensure it fits in 14 bits
        val |= (address & 0x3FFF);
    }
    ret |= val;
    return ret;
  }

  /// Helper to decode a DCC address from the first two bytes of a packet/feedback.
  /// @param b0 First byte (address byte or command byte for extended/accessory).
  /// @param b1 Second byte.
  /// @return 14-bit DCC address (0-10239) or 0xFFFF if invalid/unsupported.
  static uint16_t dcc_to_address(uint8_t b0, uint8_t b1) {
      if (b0 == 0 || b0 == 0xFF) return 0xFFFF; // Broadcast or Idle

      if ((b0 & 0xC0) == 0xC0) {
          // Long address: 11AAAAAA AAAAAAAA
          return ((b0 & 0x3F) << 8) | b1;
      } else if ((b0 & 0x80) == 0) {
          // Short address: 0AAAAAAA
          if (b0 == 0) return 0xFFFF; // Broadcast again
          return b0;
      }
      // Accessory/Extended Accessory/Consist logic could be added here if needed
      return 0xFFFF;
  }

  /// @return the event base for this node.
  uint64_t event_base() {
    uint64_t ret = FEEDBACK_EVENTID_BASE;
    ret |= ((node_->node_id() >> 24) & 0x0FFFULL) << 40;
    ret |= (node_->node_id() & 0xFFFFF) << 20;
    return ret;
  }

  void handle_identify_producer(
      const EventRegistryEntry& registry_entry, EventReport* event,
      BarrierNotifiable* done) override {
    AutoNotify an(done);
    uint64_t event_base = this->event_base();
    if (event->event < event_base) return;
    unsigned ch = (event->event - event_base) >> 16;
    if (ch >= size_) return;
    uint16_t query = event->event & 0xFFFF;
    uint16_t actual = channels_[ch].lastAddress_;
    uint64_t actual_event = address_to_eventid(ch, actual, true);
    if (query != actual) {
      event->event_write_helper<1>()->WriteAsync(
          node_, openlcb::Defs::MTI_PRODUCER_IDENTIFIED_INVALID,
          openlcb::WriteHelper::global(), openlcb::eventid_to_buffer(event->event),
          done->new_child());
    }
    event->event_write_helper<2>()->WriteAsync(
        node_, openlcb::Defs::MTI_PRODUCER_IDENTIFIED_VALID,
        openlcb::WriteHelper::global(), openlcb::eventid_to_buffer(actual_event),
        done->new_child());
  }

  void handle_identify_global(const EventRegistryEntry& registry_entry,
                              EventReport* event,
                              BarrierNotifiable* done) override {
    if (event->dst_node && event->dst_node != node_) {
      return done->notify();
    }
    // We are a producer of these events.
    uint64_t range = openlcb::EncodeRange(event_base(), size_ * 65536);
    event->event_write_helper<1>()->WriteAsync(
        node_, openlcb::Defs::MTI_PRODUCER_IDENTIFIED_RANGE, openlcb::WriteHelper::global(),
        openlcb::eventid_to_buffer(range), done->new_child());
    done->maybe_done();
  }

  dcc::RailcomHubFlow* parent_;
  openlcb::Node* node_;
  dcc::RailcomHubPortInterface* occupancyPort_;
  dcc::RailcomHubPortInterface* overcurrentPort_;
  dcc::RailcomHubPortInterface* debugPort_;
  unsigned size_;
  dcc::RailcomBroadcastDecoder* channels_;
  BarrierNotifiable n_;

  /// Tracks the presence confidence of a single locomotive on a specific channel.
  struct LocoTracker {
    uint8_t count = 0;
    static constexpr uint8_t MAX_COUNT = 10;

    /// Called when a RailCom reply is received.
    void report_loco_seen() {
      if (count <= MAX_COUNT - 2)
        count += 2;
      else
        count = MAX_COUNT;
    }

    /// Called when a DCC packet is sent.
    /// @return true if the confidence reached zero (loco missing).
    bool report_loco_addressed() {
      if (count > 0) {
        count--;
        if (count == 0) return true;
      }
      return false;
    }
  };

  /// Map of active locomotive trackers.
  /// Key: (Address << 16) | Channel.
  std::map<uint32_t, LocoTracker> trackers_;

  /// Key of the next tracker to check for timeout during iteration.
  uint32_t current_timeout_key_ = 0;

  /// Flag indicating if there are pending deletions to process.
  bool pending_deletions_ = false;

  /// Mutex protecting the trackers map and associated state.
  OSMutex lock_;
};

#endif // _BRACZ_CUSTOM_RAILCOMBROADCASTFLOW_HXX_

