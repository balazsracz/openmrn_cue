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
 * \file UpdateProcessor.cxx
 *
 * Control flow central to the command station: it round-robins between
 * refreshing the individual trains, while giving priority to the update
 * packets.
 *
 * @author Balazs Racz
 * @date 13 May 2014
 */

#include "commandstation/UpdateProcessor.hxx"

#include "utils/constants.hxx"
#include "dcc/PacketSource.hxx"
#include "dcc/PacketFlowInterface.hxx"

namespace commandstation {

// Urgent updates will take BufferBase from this pool to add stuff to the
// priorityUpdates qlist.
static Pool* urgent_update_buffer_pool() { return mainBufferPool; }

struct PriorityUpdate {
  void reset(dcc::PacketSource* source, unsigned code) {
    this->source = source;
    this->code = code;
  }
  dcc::PacketSource* source;
  unsigned code;
};

UpdateProcessor::UpdateProcessor(Service* service,
                                 dcc::PacketFlowInterface* track_send)
    : StateFlow<Buffer<dcc::Packet>, QList<1> >(service),
      trackSend_(track_send),
      nextRefreshIndex_(0),
      exclusiveIndex_(NO_EXCLUSIVE),
      hasRefreshSource_(0) {}

UpdateProcessor::~UpdateProcessor() {}

void UpdateProcessor::notify_update(dcc::PacketSource* source, unsigned code) {
  Buffer<PriorityUpdate>* b;
  urgent_update_buffer_pool()->alloc(&b, nullptr);
  HASSERT(b);
  b->data()->reset(source, code);
  AtomicHolder l(this);
  priorityUpdates_.insert(b, 0);
}

DECLARE_CONST(dcc_packet_min_refresh_delay_ms);

StateFlowBase::Action UpdateProcessor::entry() {
  // We have an empty packet to fill. It is accessible in message()->data().
  dcc::PacketSource* s = nullptr;
  Buffer<PriorityUpdate>* b = nullptr;
  {
    AtomicHolder h(this);
    // First we check if there is an exclusive update.
    if (has_exclusive()) {
      s = refreshSources_[exclusiveIndex_];
    } else {
      // Then we check if there is an urgent update.
      b = static_cast<Buffer<PriorityUpdate>*>(priorityUpdates_.next().item);
    }
  }
  long long now = os_get_time_monotonic();
  unsigned code = 0;
  if (b) {
    // found a priority entry.
    s = b->data()->source;
    code = b->data()->code;
    auto it = packetSourceStates_.find(s);
    if (it == packetSourceStates_.end()) {
      // This packet source has been removed. Do not call it!
      b->unref();
      s = nullptr;
    } else if (it->second.lastPacketTime_ >
               (now - MSEC_TO_NSEC(config_dcc_packet_min_refresh_delay_ms()))) {
      // Last update for this loco is too recent. Let's put it back to the
      // queue.
      {
        AtomicHolder h(this);
        priorityUpdates_.insert(b, 0);
      }
      s = nullptr;
    } else {
      b->unref();
    }
  }
  if (!s && hasRefreshSource_) {
    // No new update. Find the next background source.
    unsigned ntries = 0;
    while (ntries++ < refreshSources_.size()) {
      {
        AtomicHolder h(this);
        if (nextRefreshIndex_ >= refreshSources_.size()) {
          nextRefreshIndex_ = 0;
        }
        s = refreshSources_[nextRefreshIndex_++];
        code = 0;
      }
      if (packetSourceStates_[s].lastPacketTime_ <
          (now - MSEC_TO_NSEC(config_dcc_packet_min_refresh_delay_ms()))) {
        break;
      } else {
        s = nullptr;
      }
    }
  }
  if (s) {
    // requests next packet from that source.
    s->get_next_packet(code, message()->data());
    packetSourceStates_[s].lastPacketTime_ = now;
  } else {
    // No update, no source. We are idle!
    //bracz_custom::send_host_log_event(bracz_custom::HostLogEvent::TRACK_IDLE);
    message()->data()->set_dcc_idle();
  }
  // We pass on the filled packet to the track processor.
  trackSend_->send(transfer_message());
  return exit();
}

}  // namespace commandstation
