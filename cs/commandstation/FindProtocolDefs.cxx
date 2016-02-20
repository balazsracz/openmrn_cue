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
 * \file FindProtocolDefs.cxx
 *
 * Definitions for the train node find protocol.
 *
 * @author Balazs Racz
 * @date 19 Feb 2016
 */

#include "commandstation/FindProtocolDefs.hxx"
#include "commandstation/TrainDb.hxx"

namespace commandstation {

namespace {
bool is_number(char c) { return ('0' <= c) && (c <= '9'); }

bool attempt_match(const string name, unsigned pos, nmranet::EventId event) {
  int count_matches = 0;
  for (int shift = FindProtocolDefs::TRAIN_FIND_MASK - 4;
       shift >= FindProtocolDefs::TRAIN_FIND_MASK_LOW; shift -= 4) {
    uint8_t nibble = (event >> shift) & 0xf;
    if ((0 <= nibble) && (nibble <= 9)) {
      while (pos < name.size() && !is_number(name[pos])) ++pos;
      if (pos > name.size()) return false;
      if ((name[pos] - '0') != nibble) return false;
      ++pos;
      ++count_matches;
      continue;
    } else {
      continue;
    }
  }
  if (event & FindProtocolDefs::EXACT) {
    // Look for more digits in the name.
    while (pos < name.size() && !is_number(name[pos])) ++pos;
    if (pos < name.size()) return false;
  }
  return count_matches > 0;
}
}

// static
bool FindProtocolDefs::match_query_to_node(nmranet::EventId event,
                                           TrainDbEntry* train) {
  unsigned legacy_address = train->get_legacy_address();
  unsigned supplied_address = 0;
  // bool has_prefix_zero = false;
  for (int shift = TRAIN_FIND_MASK - 4; shift >= TRAIN_FIND_MASK_LOW;
       shift -= 4) {
    uint8_t nibble = (event >> shift) & 0xf;
    if (0 == nibble && 0 == supplied_address) {
      // has_prefix_zero = true;
    }
    if ((0 <= nibble) && (nibble <= 9)) {
      supplied_address *= 10;
      supplied_address += nibble;
      continue;
    }
    // For the moment we just ignore every non-numeric character. Including
    // gluing together all digits entered by the user into one big number.
  }
  if (supplied_address == legacy_address) {
    return true;
  }
  if ((event & EXACT) == 0) {
    // Search for the supplied number being a prefix of the existing addresses.
    unsigned address_prefix = legacy_address / 10;
    while (address_prefix) {
      if (address_prefix == supplied_address) {
        return true;
      }
      address_prefix /= 10;
    }
  }
  if ((event & ADDRESS_ONLY) == 0) {
    // Match against the train name string.
    string name = train->get_train_name();
    // Find the beginning of numeric components in the train name
    unsigned pos = 0;
    while (pos < name.size()) {
      if (is_number(name[pos])) {
        if (attempt_match(name, pos, event)) {
          return true;
        }
        // Skip through the sequence of numbers
        while (pos < name.size() && is_number(name[pos])) {
          ++pos;
        }
      } else {
        // non number: skip
        ++pos;
      }
    }
  }
  return false;
}

}  // namespace commandstation
