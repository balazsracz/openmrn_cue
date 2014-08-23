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
 * \file MemorizingEventHandler.cxx
 *
 * A set of event handler that allow remembering state of the layout (as
 * represented by events), saving it to a file, and restoring if anyone is
 * asking.
 *
 * @author Balazs Racz
 * @date 23 Aug 2014
 */

#include "custom/MemorizingEventHandler.hxx"
#include "nmranet/WriteHelper.hxx"

namespace nmranet {

MemorizingHandlerManager::MemorizingHandlerManager(Node* node,
                                                   uint64_t event_base,
                                                   unsigned num_total_events,
                                                   unsigned block_size)
    : node_(node),
      event_base_(event_base),
      num_total_events_(num_total_events),
      block_size_(block_size) {
  unsigned mask = EventRegistry::align_mask(&event_base, num_total_events);
  EventRegistry::instance()->register_handlerr(this, event_base, mask);
  HASSERT(num_total_events % block_size == 0);
}

MemorizingHandlerManager::~MemorizingHandlerManager() {
  uint64_t base = event_base_;
  unsigned mask = EventRegistry::align_mask(&base, num_total_events_);
  EventRegistry::instance()->unregister_handlerr(this, base, mask);
}

void MemorizingHandlerManager::HandleEventReport(
    EventReport* event, BarrierNotifiable* done) OVERRIDE {
  AutoNotify n(done);
  if (!is_mine(event->event)) return;
  UpdateValidEvent(event->event);
}

void MemorizingHandlerManager::HandleConsumerIdentified(
    EventReport* event, BarrierNotifiable* done) OVERRIDE {
  AutoNotify n(done);
  if (!is_mine(event->event) || event->state != VALID) return;
  UpdateValidEvent(event->event);
}

void MemorizingHandlerManager::HandleProducerIdentified(
    EventReport* event, BarrierNotifiable* done) OVERRIDE {
  AutoNotify n(done);
  if (!is_mine(event->event) || event->state != VALID) return;
  UpdateValidEvent(event->event);
}

void MemorizingHandlerManager::UpdateValidEvent(uint64_t eventid) {
  unsigned block_num = (eventid - event_base_) / block_size_;
  unsigned block_base = block_num * block_size_;
  // We don't need locking anywhere here because this will be run on the
  // executor that is responsible for event handling.
  auto& p = blocks_[block_base];
  if (!p) {
    p.reset(new MemorizingHandlerBlock(this, event_base_ + block_base));
  }
  p->set_current_event(eventid);
}

MemorizingHandlerBlock::MemorizingHandlerBlock(MemorizingHandlerManager* parent,
                                               uint64_t event_base)
    : parent_(parent), event_base_(event_base), current_event_(0) {
  unsigned mask = EventRegistry::align_mask(&event_base, parent_->block_size());
  EventRegistry::instance()->register_handlerr(this, event_base, mask);
}

MemorizingHandlerBlock::~MemorizingHandlerBlock() {
  uint64_t base = event_base_;
  unsigned mask = EventRegistry::align_mask(&base, parent_->block_size());
  EventRegistry::instance()->unregister_handlerr(this, base, mask);
}

/** Checks if a single eventid is ours, and sends out a PCER for the valid
 * event. Notifies done. */
void MemorizingHandlerBlock::ReportSingle(uint64_t eventid,
                                          BarrierNotifiable* done) {
  AutoNotify n(done);
  if (!is_mine(eventid)) return;
  event_write_helper1.WriteAsync(
      parent_->node(), Defs::MTI_EVENT_REPORT, WriteHelper::global(),
      eventid_to_buffer(current_event_), done->new_child());
}

/** Checks if the eventid is ours. Sends an event report with the currently
 * valid eventid, and an Identified_{producer/consumer}_{valid/invalid} for
 * the given eventid. */
void MemorizingHandlerBlock::ReportAndIdentify(uint64_t eventid, Defs::MTI mti,
                                               BarrierNotifiable* done) {
  AutoNotify n(done);
  if (!is_mine(eventid)) return;
  if (eventid != current_event_) {
    mti++;
  }
  event_write_helper2.WriteAsync(parent_->node(), mti, WriteHelper::global(),
                                 eventid_to_buffer(eventid), done->new_child());
  event_write_helper1.WriteAsync(
      parent_->node(), Defs::MTI_EVENT_REPORT, WriteHelper::global(),
      eventid_to_buffer(current_event_), done->new_child());
}

/** Checks if a range intersects with our range; if so, then produces the
 * valid event report. */
void MemorizingHandlerBlock::ReportRange(EventReport* event,
                                         BarrierNotifiable* done) {
  AutoNotify n(done);
  if (!is_mine_range(event)) return;
  event_write_helper1.WriteAsync(
      parent_->node(), Defs::MTI_EVENT_REPORT, WriteHelper::global(),
      eventid_to_buffer(current_event_), done->new_child());
}

}  // namespace nmranet
