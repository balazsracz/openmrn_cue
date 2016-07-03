/** \copyright
 * Copyright (c) 2014-2016, Balazs Racz
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
 * \file FindProtocolDefs.hxx
 *
 * Definitions for the train node find protocol.
 *
 * @author Balazs Racz
 * @date 18 Feb 2016
 */

#ifndef _COMMANDSTATION_FINDPROTOCOLDEFS_HXX_
#define _COMMANDSTATION_FINDPROTOCOLDEFS_HXX_

#include "nmranet/EventHandler.hxx"
#include "commandstation/TrainDbDefs.hxx"

namespace commandstation {

class TrainDbEntry;

struct FindProtocolDefs {
  // static constexpr EventID
  enum {
    TRAIN_FIND_BASE = 0x090099FF00000000U,
  };

  // Command byte definitions
  enum {
    // What is the mask value for the event registry entry.
    TRAIN_FIND_MASK = 32,
    // Where does the command byte start.
    TRAIN_FIND_MASK_LOW = 8,

    ALLOCATE = 0x80,
    // SEARCH = 0x00,

    EXACT = 0x40,
    // SUBSTRING = 0x00,

    ADDRESS_ONLY = 0x20,
    // ADDRESS_NAME_CABNUMBER = 0x00

    DCC_FORCE_LONG = 0x10,

    // Bits 0-2 are a DccMode enum.
    
    // Match response information. This bit is not used in the network
    // protocol. The bit will be set in the matched result, while cleared for a
    // no-match.
    MATCH_ANY = 0x01,
  };

  static_assert((TRAIN_FIND_BASE & ((1ULL << TRAIN_FIND_MASK) - 1)) == 0,
                "TRAIN_FIND_BASE is not all zero on the bottom");

  // Search nibble definitions.
  enum {
    NIBBLE_UNUSED = 0xf,
    NIBBLE_SPACE = 0xe,
    NIBBLE_STAR = 0xd,
    NIBBLE_QN = 0xc,
    NIBBLE_HASH = 0xb,
  };

  /** Compares an incoming search query to a given train node. Returns 0 for a
      no-match. Returns a bitfield of match types for a match. valid bits are
      MATCH_ANY (always set), ADDRESS_ONLY (set when the match occurred in the
      address), EXACT (clear for prefix match). */
  static uint8_t match_query_to_node(nmranet::EventId event, TrainDbEntry* train);

  /** Converts a find protocol query to an address and desired DccMode
      information. Will take into account prefix zeros for forcing a dcc long
      address, as well as all mode and flag bits coming in via the query.
      
      @param mode (can't be null) will be filled in with the Dcc Mode: the
      bottom 3 bits as specified by the incoming query, or zero if the query
      did not specify a preference. If the query started with a prefix of zero
      (typed by the user) or DCC_FORCE_LONG_ADDRESS was set in the query, the
      DccMode will have the force long address bit set.

      @returns the new legacy_address. */
  static unsigned query_to_address(nmranet::EventId query, DccMode* mode);

  /** Translates an address as punched in by a (dumb) throttle to a query to
   * issue on the OpenLCB bus as a find protocol request.
   *
   * @param address is the numeric value that the user typed.
   * @param exact should be true if only exact matches shall be retrieved.
   * @param mode should be set most of the time to OLCBUSER to specify that we
   * don't care about the address type, but can also be set to
   * DCC_LONG_ADDRESS.
   */
  static nmranet::EventId address_to_query(unsigned address, bool exact, DccMode mode);

 private:
  // Not instantiatable class.
  FindProtocolDefs();
};

}  // namespace commandstation

#endif  // _COMMANDSTATION_FINDPROTOCOLDEFS_HXX_
