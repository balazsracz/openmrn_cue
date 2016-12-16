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
#include "nmranet/EventHandlerTemplates.hxx"

namespace openlcb {

MemorizingHandlerManager::MemorizingHandlerManager(Node* node,
                                                   uint64_t event_base,
                                                   unsigned num_total_events,
                                                   unsigned block_size)
    : node_(node),
      event_base_(event_base),
      num_total_events_(num_total_events),
      block_size_(block_size) {
  unsigned mask = EventRegistry::align_mask(&event_base, num_total_events);
  EventRegistry::instance()->register_handler(
      EventRegistryEntry(this, event_base), mask);
  HASSERT(num_total_events % block_size == 0);
}

MemorizingHandlerManager::~MemorizingHandlerManager() {
  EventRegistry::instance()->unregister_handler(this);
}

void MemorizingHandlerManager::handle_event_report(
    const EventRegistryEntry& registry_entry, EventReport* event,
    BarrierNotifiable* done) {
  AutoNotify n(done);
  if (!is_mine(event->event)) return;
  UpdateValidEvent(event->event);
}

void MemorizingHandlerManager::handle_consumer_identified(
    const EventRegistryEntry& registry_entry, EventReport* event,
    BarrierNotifiable* done) {
  AutoNotify n(done);
  if (!is_mine(event->event) || event->state != EventState::VALID) return;
  UpdateValidEvent(event->event);
}

void MemorizingHandlerManager::handle_producer_identified(
    const EventRegistryEntry& registry_entry, EventReport* event,
    BarrierNotifiable* done) {
  AutoNotify n(done);
  if (!is_mine(event->event) || event->state != EventState::VALID) return;
  UpdateValidEvent(event->event);
}

void MemorizingHandlerManager::handle_identify_global(
    const EventRegistryEntry& registry_entry, EventReport* event,
    BarrierNotifiable* done) {
  AutoNotify n(done);
  uint64_t range = EncodeRange(event_base_, num_total_events_);
  event_write_helper1.WriteAsync(node_, Defs::MTI_PRODUCER_IDENTIFIED_RANGE,
                                 WriteHelper::global(),
                                 eventid_to_buffer(range), done->new_child());
  event_write_helper2.WriteAsync(node_, Defs::MTI_CONSUMER_IDENTIFIED_RANGE,
                                 WriteHelper::global(),
                                 eventid_to_buffer(range), done->new_child());
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

struct MemorizingHandlerManager::BlockOffsetInfo {
  // offset in the file
  off_t file_offset;
  // how many bytes from that offset need to be read. Must be between 1 and
  // 64. The bytes are stored in host-endian byte order.
  uint8_t read_bytes;
  // how many bits the data read needs to be shifted so that we get the eventid.
  uint8_t shift_count;
  // This is how many of the low bits are actually used (need to be
  // masked). e.g. if this value is 3, then we need to mask with & 7.
  uint8_t bits_used;
};

void MemorizingHandlerManager::GetBlockFileOffset(unsigned block_num,
                                                  BlockOffsetInfo* info) {
  HASSERT(info);
  uint8_t bits_used = 2;
  unsigned max_size = 4;
  // we need one additional state (for 'unknown') than the actual block size.
  while (block_size() >= max_size && bits_used < 32) {
    max_size <<= 1;
    bits_used++;
  }
  info->bits_used = bits_used;
  unsigned bit_offset = block_num * bits_used;
  info->file_offset = file_offset_ + (bit_offset >> 3);
  info->shift_count = bit_offset & 7;
  info->read_bytes = (info->shift_count + bits_used + 7) >> 3;
  HASSERT(info->read_bytes <= 8);
}

size_t read_repeated(int fd, void* d, size_t bytes) {
  uint8_t* data = (uint8_t*)d;
  size_t rd = 0;
  while (bytes) {
    ssize_t ret = ::read(fd, data, bytes);
    ERRNOCHECK("read", ret);
    if (ret == 0) {
      memset(data, 0, bytes);
      return rd;
    }
    data += ret;
    rd += ret;
    bytes -= ret;
  }
  return rd;
}

void write_repeated(int fd, const void* d, size_t bytes) {
  const uint8_t* data = (uint8_t*)d;
  size_t rd = 0;
  while (bytes) {
    ssize_t ret = ::write(fd, data, bytes);
    ERRNOCHECK("write", ret);
    data += ret;
    rd += ret;
    bytes -= ret;
  }
}

uint64_t MemorizingHandlerManager::GetBlockFromFile(unsigned block_num) {
  BlockOffsetInfo info;
  GetBlockFileOffset(block_num, &info);
  off_t offset = lseek(fd_, info.file_offset, SEEK_SET);
  if (offset < info.file_offset) {
    // file does not have data about this
    return 0;
  }
  uint64_t data = 0;
  read_repeated(fd_, &data, info.read_bytes);
  data >>= info.shift_count;
  data &= (1ULL << info.bits_used) - 1;
  // special marker of zeroes: state unknown
  if (!data) return 0;
  --data;
  uint64_t event_id = event_base_ + block_num * block_size_ + data;
  return event_id;
}

void MemorizingHandlerManager::SaveValidEventToFile(uint64_t eventid) {
  unsigned block_num = (eventid - event_base_) / block_size_;
  unsigned block_base = block_num * block_size_;
  uint64_t block_value = eventid - event_base_ - block_base;

  BlockOffsetInfo info;
  GetBlockFileOffset(block_num, &info);
  off_t offset = lseek(fd_, info.file_offset, SEEK_SET);
  ERRNOCHECK("lseek", offset);
  HASSERT(offset == info.file_offset);

  ++block_value;  // zero is reserved for "unknown" so we shift everything else.
  uint64_t mask = ((1ULL << info.bits_used) - 1);
  HASSERT((block_value & mask) == block_value);
  uint64_t data = 0;
  read_repeated(fd_, &data, info.read_bytes);
  block_value <<= info.shift_count;
  mask <<= info.shift_count;
  data &= ~mask;
  data |= block_value;

  offset = lseek(fd_, info.file_offset, SEEK_SET);
  ERRNOCHECK("lseek", offset);
  HASSERT(offset == info.file_offset);
  write_repeated(fd_, &data, info.read_bytes);
}

MemorizingHandlerBlock::MemorizingHandlerBlock(MemorizingHandlerManager* parent,
                                               uint64_t event_base)
    : parent_(parent), event_base_(event_base), current_event_(0) {
  unsigned mask = EventRegistry::align_mask(&event_base, parent_->block_size());
  EventRegistry::instance()->register_handler(
      EventRegistryEntry(this, event_base), mask);
}

MemorizingHandlerBlock::~MemorizingHandlerBlock() {
  EventRegistry::instance()->unregister_handler(this);
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
  // This will happen by us listening to our own identified call.
  /*event_write_helper1.WriteAsync(
      parent_->node(), Defs::MTI_EVENT_REPORT, WriteHelper::global(),
      eventid_to_buffer(current_event_), done->new_child());*/
}

/** Checks if a range intersects with our range; if so, then produces the
 * valid event report. */
void MemorizingHandlerBlock::ReportRange(EventReport* event,
                                         BarrierNotifiable* done) {
  AutoNotify n(done);
  if (!is_mine_range(event)) return;
  // We don't respond to range queries from ourselves; this should prevent us
  // from generating event reports on the global identify message.
  if (event->src_node.id == parent_->node()->node_id()) return;
  event_write_helper1.WriteAsync(
      parent_->node(), Defs::MTI_EVENT_REPORT, WriteHelper::global(),
      eventid_to_buffer(current_event_), done->new_child());
}

}  // namespace openlcb
