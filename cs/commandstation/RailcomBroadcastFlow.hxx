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
 * \file LoggingBit.hxx
 *
 * Simple Bit event implementation that logs to the blinker or stderr.
 *
 * @author Balazs Racz
 * @date 13 Dec 2015
 */

#ifndef _BRACZ_CUSTOM_RAILCOMBROADCASTFLOW_HXX_
#define _BRACZ_CUSTOM_RAILCOMBROADCASTFLOW_HXX_

#include "dcc/RailcomHub.hxx"
#include "dcc/RailcomBroadcastDecoder.hxx"

class RailcomBroadcastFlow : public dcc::RailcomHubPort {
 public:
  RailcomBroadcastFlow(dcc::RailcomHubFlow* parent, nmranet::Node* node,
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
  }

  ~RailcomBroadcastFlow() {
    parent_->unregister_port(this);
    delete[] channels_;
  }

  Action entry() {
    auto channel = message()->data()->channel;
    if (channel == 0xff) {
      uint32_t new_values = message()->data()->ch1Data[0];
      for (unsigned i = 0; i < size_; ++i) {
        channels_[i].set_occupancy(new_values & (1 << i));
        auto& decoder = channels_[i];
        if (decoder.current_address() != decoder.lastAddress_) {
          // We need to send a fake railcom packet to ourselves to allow the
          // channel empty events to be generated.
          auto* b = alloc();
          b->data()->reset(0);
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
    b->data()->reset(nmranet::Defs::MTI_PRODUCER_IDENTIFIED_INVALID,
                     node_->node_id(),
                     nmranet::eventid_to_buffer(
                         address_to_eventid(channel, decoder.lastAddress_)));
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
    b->data()->reset(nmranet::Defs::MTI_EVENT_REPORT, node_->node_id(),
                     nmranet::eventid_to_buffer(address_to_eventid(
                         channel, decoder.current_address())));
    b->set_done(n_.reset(this));
    node_->iface()->global_message_write_flow()->send(b);
    decoder.lastAddress_ = decoder.current_address();
    release();
    return wait_and_call(STATE(finish));
  }

  Action finish() { return exit(); }

 private:
  static const uint64_t FEEDBACK_EVENTID_BASE;
  uint64_t address_to_eventid(unsigned channel, uint16_t address) {
    uint64_t ret = FEEDBACK_EVENTID_BASE;
    ret |= uint64_t(channel & 0xff) << 48;
    ret |= address;
    return ret;
  }

  dcc::RailcomHubFlow* parent_;
  nmranet::Node* node_;
  dcc::RailcomHubPortInterface* occupancyPort_;
  dcc::RailcomHubPortInterface* overcurrentPort_;
  dcc::RailcomHubPortInterface* debugPort_;
  unsigned size_;
  dcc::RailcomBroadcastDecoder* channels_;
  BarrierNotifiable n_;
};

const uint64_t RailcomBroadcastFlow::FEEDBACK_EVENTID_BASE =
    (0x09ULL << 56) | nmranet::TractionDefs::NODE_ID_DCC;

#endif // _BRACZ_CUSTOM_RAILCOMBROADCASTFLOW_HXX_

