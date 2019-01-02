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
 * \file MemorizingEventHandler.hxx
 *
 * A set of event handler that allow remembering state of the layout (as
 * represented by events), saving it to a file, and restoring if anyone is
 * asking.
 *
 * @author Balazs Racz
 * @date 23 Aug 2014
 */

#ifndef _BRACZ_CUSTOM_MEMORIZINGEVENTHANDLER_HXX_
#define _BRACZ_CUSTOM_MEMORIZINGEVENTHANDLER_HXX_

#include <memory>
#include <map>

#include "openlcb/EventHandler.hxx"
#include "openlcb/Defs.hxx"

namespace openlcb {

class MemorizingHandlerBlock;

/** A memorizing handler manager is responsible for on-demand creating
 * memorizing event handler blocks covering a large event interval. Each
 * handler block will keep the state of an event-based variable. The variable
 * is represented as a block of K consecutive events, of which only one can
 * ever be valid, all others are invalid. The event handler block will remember
 * which was the last produced event. If a producer or consumer identified
 * message arrives for the any of the events in the block, the memorizing
 * handler will emit the last known state of the block as an event, in addition
 * to responding with valid/invalid.
 *
 * Example: block size of 2 represents the traditional on/off state of a single
 * bit variable. Say event base = 0x050101011422FF00, and we have bit variables
 * for a total size of 128 (0x80).
 *
 * When event 0x050101011422FF30 arrives, the manager will create the block for
 * {FF30, FF31}, and remember that the valid offset is FF30.
 *
 * If at this point an IdentifyProducer for FF31 arrives, the block will
 * respond ProducerIdentified False, and Event Report FF30. This will ensure
 * that whoever was inquiring about the state of the variable will get the
 * proper state.
 *
 */
class MemorizingHandlerManager : public EventHandler {
 public:
  /** Creates a memorizing handler block. It will register itself with the
   * global event registry.
   *
   * @param event_base is the first event of the first block.
   * @param num_total_events is the total size of all the blocks.
   * @param block_size tells how many concecutive events is in one
   * block. num_total_events must be divisible by block_size.
   */
  MemorizingHandlerManager(Node* node, uint64_t event_base,
                           unsigned num_total_events, unsigned block_size);
  ~MemorizingHandlerManager();

  void handle_event_report(const EventRegistryEntry& registry_entry,
                           EventReport* event, BarrierNotifiable* done) OVERRIDE;
  void handle_consumer_identified(const EventRegistryEntry& registry_entry,
                                EventReport* event,
                                BarrierNotifiable* done) OVERRIDE;
  void handle_producer_identified(const EventRegistryEntry& registry_entry,
                                EventReport* event,
                                BarrierNotifiable* done) OVERRIDE;
  void handle_identify_global(const EventRegistryEntry& registry_entry,
                            EventReport* event,
                            BarrierNotifiable* done) OVERRIDE;
  void handle_identify_consumer(const EventRegistryEntry& registry_entry,
                              EventReport* event,
                              BarrierNotifiable* done) override {
    // These are handled by individual blocks TODO: what if there is no block
    return done->notify();
  }
  void handle_identify_producer(const EventRegistryEntry& registry_entry,
                              EventReport* event,
                              BarrierNotifiable* done) override {
    // These are handled by individual blocks TODO: what if there is no block
    return done->notify();
  }

  unsigned block_size() { return block_size_; }

  Node* node() { return node_; }

 private:
  /** @returns true if the event report is in the range we are responsible
   * for. */
  bool is_mine(uint64_t event) {
    return event >= event_base_ && event < (event_base_ + num_total_events_);
  }

  /// Reports that the given event ID is in the VALID state. Internally it
  /// creates the handler block if it does not exist yet. This will be called
  /// on both PCER, as well as Identify {Producer, Consumer} VALID messages.
  void UpdateValidEvent(uint64_t eventid);

  struct BlockOffsetInfo;
  inline void GetBlockFileOffset(unsigned block_num, BlockOffsetInfo* info);

  /// Checks the backing file whether the given block has information saved or
  /// not. If it has info, returns the valid event for that block. Otherwise
  /// returns 0.
  uint64_t GetBlockFromFile(unsigned block_offset);

  /// Sets a block in the backing file to a given valid event id.
  void SaveValidEventToFile(uint64_t eventid);

  Node* node_;
  uint64_t event_base_;
  unsigned num_total_events_;
  unsigned block_size_;
  int fd_{-1};  /// If >= 0, then our block is backed by a file.
  unsigned file_offset_;

  // All the event blocks we own. Keyed by (first_event_of_block - event_base_).
  std::map<unsigned, std::unique_ptr<MemorizingHandlerBlock> > blocks_;
};

class MemorizingHandlerBlock : public EventHandler {
 public:
  // Creates an event handler block, and registers it.
  MemorizingHandlerBlock(MemorizingHandlerManager* parent, uint64_t event_base);
  ~MemorizingHandlerBlock();

  // Event reports are handled by the manager.
  void handle_event_report(const EventRegistryEntry& registry_entry,
                           EventReport* event, BarrierNotifiable* done) OVERRIDE {
    done->notify();
  }

  void handle_consumer_identified(const EventRegistryEntry& registry_entry,
                                EventReport* event,
                                BarrierNotifiable* done) OVERRIDE {
    ReportSingle(event, done);
  }
  void handle_consumer_range_identified(const EventRegistryEntry& registry_entry,
                                     EventReport* event,
                                     BarrierNotifiable* done) OVERRIDE {
    ReportRange(event, done);
  }
  void handle_producer_identified(const EventRegistryEntry& registry_entry,
                                EventReport* event,
                                BarrierNotifiable* done) OVERRIDE {
    ReportSingle(event, done);
  }
  void handle_producer_range_identified(const EventRegistryEntry& registry_entry,
                                     EventReport* event,
                                     BarrierNotifiable* done) OVERRIDE {
    ReportRange(event, done);
  }
  void handle_identify_producer(const EventRegistryEntry& registry_entry,
                              EventReport* event,
                              BarrierNotifiable* done) OVERRIDE {
    ReportAndIdentify(event, Defs::MTI_PRODUCER_IDENTIFIED_VALID, done);
  }
  void handle_identify_consumer(const EventRegistryEntry& registry_entry,
                              EventReport* event,
                              BarrierNotifiable* done) OVERRIDE {
    ReportAndIdentify(event, Defs::MTI_CONSUMER_IDENTIFIED_VALID, done);
  }

  void handle_identify_global(const EventRegistryEntry& registry_entry,
                            EventReport* event,
                            BarrierNotifiable* done) OVERRIDE {
    // Global identify is handler by the block manager.
    return done->notify();
  }

  void set_current_event(uint64_t e) { current_event_ = e; }

  uint64_t current_event() { return current_event_; }

 private:
  /** Checks if a single eventid is ours, and sends out a PCER for the valid
   * event. Notifies done. */
  void ReportSingle(EventReport* event, BarrierNotifiable* done);

  /** Checks if the eventid is ours. Sends an event report with the currently
   * valid eventid, and an Identified_{producer/consumer}_{valid/invalid} for
   * the given eventid. */
  void ReportAndIdentify(EventReport* event, Defs::MTI mti,
                         BarrierNotifiable* done);

  /** Checks if a range intersects with our range; if so, then produces the
   * valid event report. */
  void ReportRange(EventReport* event, BarrierNotifiable* done);

  /** @returns true if the eventid is in my range. */
  bool is_mine(uint64_t eventid) {
    return eventid >= event_base_ &&
           eventid < (event_base_ + parent_->block_size());
  }

  /** @returns true if a range intersects my range. */
  bool is_mine_range(EventReport* event) {
    return event->event < (event_base_ + parent_->block_size()) &&
           (event->event | event->mask) >= event_base_;
  }

  MemorizingHandlerManager* parent_;
  uint64_t event_base_;
  uint64_t current_event_;
};

}  // namespace openlcb

#endif  // _BRACZ_CUSTOM_MEMORIZINGEVENTHANDLER_HXX_
