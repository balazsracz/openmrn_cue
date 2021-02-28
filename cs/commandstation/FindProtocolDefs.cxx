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
#include "commandstation/ExternalTrainDbEntry.hxx"
#include "openlcb/TractionDefs.hxx"

namespace commandstation {

/// Specifies what kind of train to allocate when the drive mode is left as
/// default / unspecified.
uint8_t FindProtocolDefs::DEFAULT_DRIVE_MODE = DCC_128;

/// Specifies what kind of train to allocate when the drive mode is set as
/// MARKLIN_ANY.
uint8_t FindProtocolDefs::DEFAULT_MARKLIN_DRIVE_MODE = MARKLIN_NEW;

/// Specifies what kind of train to allocate when the drive mode is set as
/// DCC_ANY.
uint8_t FindProtocolDefs::DEFAULT_DCC_DRIVE_MODE = DCC_128;

#ifndef GTEST
namespace {
#endif
/// @returns true for a character that is a digit.
bool is_number(char c) { return ('0' <= c) && (c <= '9'); }

/// @return true if the given nibble is a digit nibble (0 to 9)
bool is_digit_nibble(unsigned nibble) {
  return ((0 <= nibble) && (nibble <= 9));
}

/// @returns true if the user has any digits as a query.
bool has_query(openlcb::EventId event) {
  for (int shift = FindProtocolDefs::TRAIN_FIND_MASK - 4;
       shift >= FindProtocolDefs::TRAIN_FIND_MASK_LOW; shift -= 4) {
    uint8_t nibble = (event >> shift) & 0xf;
    if ((0 <= nibble) && (nibble <= 9)) {
      return true;
    }
  }
  return false;
}

#if 0
/// @returns the same bitmask as match_query_to_node.
uint8_t attempt_match(const string& name, unsigned pos, openlcb::EventId event) {
  int count_matches = 0;
  for (int shift = FindProtocolDefs::TRAIN_FIND_MASK - 4;
       shift >= FindProtocolDefs::TRAIN_FIND_MASK_LOW; shift -= 4) {
    uint8_t nibble = (event >> shift) & 0xf;
    if ((0 <= nibble) && (nibble <= 9)) {
      while (pos < name.size() && !is_number(name[pos])) ++pos;
      if (pos > name.size()) return 0;
      if ((name[pos] - '0') != nibble) return 0;
      ++pos;
      ++count_matches;
      continue;
    } else {
      continue;
    }
  }
  // If we are here, we have exhausted (and matched) all digits that were sent
  // by the query.
  if (count_matches == 0) {
    // Query had no digits. This is handled elsewhere.
    return 0;
  }
  // Look for more digits in the name.
  while (pos < name.size() && !is_number(name[pos])) ++pos;
  if (pos < name.size()) {
    if (event & FindProtocolDefs::EXACT) {
      return 0;
    }
    return FindProtocolDefs::MATCH_ANY;
  } else {
    return FindProtocolDefs::EXACT | FindProtocolDefs::MATCH_ANY;
  }
}
#endif

/// Check if a query term matched the name.
/// @param name the train name
/// @param event id the query event
/// @param shift_start a digit nibble in the query which to match first
/// @param shift_end a digit nibble in the query this is the last digit of the
/// query term
/// @return 0 if there was no match, MATCH_ANY if there was a prefix match and
/// the query was not EXACT_ONLY, or MATCH_ANY | EXACT if a set of digits in
/// the name matched exactly on the query term.
uint8_t attempt_name_match_term(const string& name, openlcb::EventId event,
                                unsigned shift_start, unsigned shift_end) {
  bool prev_is_digit = false;
  unsigned shift_next = 0;
  bool match_ok = false;
  bool found_match = false;
  for (unsigned pos = 0; pos < name.size(); ++pos) {
    if (!is_number(name[pos])) {
      prev_is_digit = false;
      continue;
    }
    if (!prev_is_digit) {
      // starting match from the beginning of the term
      shift_next = shift_start;
      match_ok = true;
    }
    prev_is_digit = true;
    uint8_t nibble = (event >> shift_next) & 0xf;
    if (!match_ok || (name[pos] - '0') != nibble) {
      match_ok = false;
      continue;
    }
    if (shift_next <= shift_end) {
      // completed match.
      if (pos + 1 >= name.size() || !is_number(name[pos + 1])) {
        // exact match
        return FindProtocolDefs::EXACT | FindProtocolDefs::MATCH_ANY;
      } else if ((event & FindProtocolDefs::EXACT) == 0) {
        found_match = true;
        // This will skip all further digits until another non-digit to digit
        // transition starts the match from the beginning.
        match_ok = false;
      }
    } else {
      shift_next -= 4;
    }
  }
  if (found_match) {
    return FindProtocolDefs::MATCH_ANY;
  }
  return 0;
}

/// Checks if a query matched the name. Takes into account the ADDRESS_ONLY bit
/// (returns 0 in this case) and also the EXACT bit.
/// @param name the train name
/// @param event the query event.
/// @return 0, MATCH_ANY, or MATCH_ANY | EXACT
uint8_t attempt_name_match(const string& name, openlcb::EventId event) {
  if (event & FindProtocolDefs::ADDRESS_ONLY) {
    return 0;
  }
  uint8_t best_match = 0xff;
  // find sequences digit nibbles and iterate over them.
  int shift = FindProtocolDefs::TRAIN_FIND_MASK - 4;
  while (shift >= FindProtocolDefs::TRAIN_FIND_MASK_LOW) {
    uint8_t nibble = (event >> shift) & 0xf;
    if (is_digit_nibble(nibble)) {
      // This is the start of a query term.
      int shift_start = shift;
      while (shift > FindProtocolDefs::TRAIN_FIND_MASK_LOW &&
             is_digit_nibble((event >> (shift - 4)) & 0xf)) {
        shift -= 4;
      }
      auto term_ret = attempt_name_match_term(name, event, shift_start, shift);
      if (!term_ret) {
        // we have a term that did not match.
        return 0;
      }
      best_match &= term_ret;
    }
    shift -= 4;
  }
  if (best_match == 0xff) {
    // There were no query terms.
    return 0;
  }
  return best_match;
}

#ifndef GTEST
}  // anonymous namespace
#endif

// static
bool FindProtocolDefs::match_event_to_drive_mode(openlcb::EventId event,
                                                 DccMode mode) {
  DccMode req_mode = (DccMode)(event & DCCMODE_PROTOCOL_MASK);
  if (req_mode == DCCMODE_DEFAULT) {
    return true;
  }
  if (mode == DCCMODE_DEFAULT) {
    return true;
  }
  return dcc_mode_to_protocol(mode) == dcc_mode_to_protocol(req_mode);
}

// static
unsigned FindProtocolDefs::query_to_address(openlcb::EventId event,
                                            DccMode* mode) {
  unsigned supplied_address = 0;
  bool has_prefix_zero = false;
  for (int shift = TRAIN_FIND_MASK - 4; shift >= TRAIN_FIND_MASK_LOW;
       shift -= 4) {
    uint8_t nibble = (event >> shift) & 0xf;
    if (0 == nibble && 0 == supplied_address) {
      has_prefix_zero = true;
    }
    if ((0 <= nibble) && (nibble <= 9)) {
      supplied_address *= 10;
      supplied_address += nibble;
      continue;
    }
    // For the moment we just ignore every non-numeric character. Including
    // gluing together all digits entered by the user into one big number.
  }
  uint8_t drive_type = event & DCCMODE_PROTOCOL_MASK;
  if (event & ALLOCATE) {
    // If we are allocating, then we fill in defaults for drive modes.
    if (drive_type == DCCMODE_DEFAULT) {
      drive_type = DEFAULT_DRIVE_MODE;
    } else if (drive_type == MARKLIN_DEFAULT) {
      drive_type = DEFAULT_MARKLIN_DRIVE_MODE;
    } else if (drive_type == DCC_DEFAULT) {
      drive_type = DEFAULT_DCC_DRIVE_MODE;
    } else if (drive_type == (DCC_DEFAULT | DCC_LONG_ADDRESS)) {
      drive_type |= (DEFAULT_DCC_DRIVE_MODE & DCC_SS_MASK);
    }
  }
  
  if (has_prefix_zero &&  //
      (((drive_type & DCC_ANY_MASK) == DCC_ANY) || (drive_type == DCCMODE_DEFAULT))) {
    drive_type |= commandstation::DCC_DEFAULT | commandstation::DCC_LONG_ADDRESS;
  }
  *mode = static_cast<DccMode>(drive_type);
  return supplied_address;
}

// static
openlcb::EventId FindProtocolDefs::address_to_query(unsigned address,
                                                    bool exact, DccMode mode) {
  uint64_t event = TRAIN_FIND_BASE;
  int shift = TRAIN_FIND_MASK_LOW;
  while (address) {
    event |= (address % 10) << shift;
    shift += 4;
    address /= 10;
  }
  while (shift < TRAIN_FIND_MASK) {
    event |= UINT64_C(0xF) << shift;
    shift += 4;
  }
  if (exact) {
    event |= EXACT;
  }
  event |= mode & DCCMODE_PROTOCOL_MASK;
  return event;
}

// static
uint8_t FindProtocolDefs::match_query_to_node(openlcb::EventId event,
                                              TrainDbEntry* train) {
  // empty search should match everything.
  if (event == openlcb::TractionDefs::IS_TRAIN_EVENT) {
    return MATCH_ANY | ADDRESS_ONLY | EXACT;
  }
  unsigned legacy_address = train->get_legacy_address();
  DccMode mode;
  unsigned supplied_address = query_to_address(event, &mode);
  bool req_has_query = has_query(event);
  bool has_address_prefix_match = false;
  auto actual_drive_mode = train->get_legacy_drive_mode();
  auto desired_address_type = dcc_mode_to_address_type(mode, supplied_address);
  auto actual_address_type =
      dcc_mode_to_address_type(actual_drive_mode, legacy_address);
  bool address_type_match =
      (actual_address_type == dcc::TrainAddressType::UNSUPPORTED ||
       desired_address_type == dcc::TrainAddressType::UNSPECIFIED ||
       desired_address_type == actual_address_type);
  if (!match_event_to_drive_mode(event, actual_drive_mode)) {
    // The request specified a restriction on the locomotive mode and this
    // restriction does not match the current loco.
    return 0;
  }
  if (supplied_address == legacy_address) {
    if (address_type_match) {
      // If the caller did not specify the drive mode, or the drive mode
      // matches.
      return MATCH_ANY | ADDRESS_ONLY | EXACT;
    } else {
      LOG(INFO, "exact match failed due to mode: desired %d actual %d",
          static_cast<int>(desired_address_type), static_cast<int>(actual_address_type));
    }
  }
  if (((event & EXACT) == 0) && address_type_match) {
    // Search for the supplied number being a prefix of the existing addresses.
    unsigned address_prefix = legacy_address / 10;
    while (address_prefix) {
      if (address_prefix == supplied_address) {
        has_address_prefix_match = true;
        break;
      }
      address_prefix /= 10;
    }
  }
  if ((mode != DCCMODE_DEFAULT) && (event & EXACT) && (event & ALLOCATE) && (event & ADDRESS_ONLY)) {
    // Request specified a drive mode and allocation. We check the drive mode
    // to match.
    if (desired_address_type != actual_address_type) {
      return 0;
    }
  }
  if (event & ADDRESS_ONLY) {
    if (((event & EXACT) != 0) || (!has_address_prefix_match)) return 0;
    return MATCH_ANY | ADDRESS_ONLY;
  }
  if (!req_has_query) {
    if ((event & EXACT) != 0) return 0;
    return MATCH_ANY;
  }
  string name = train->get_train_name();
  uint8_t best_name_match = attempt_name_match(name, event);
  if (((best_name_match & EXACT) == 0) && has_address_prefix_match &&
      ((event & EXACT) == 0)) {
    // We prefer a partial address match over a non-exact name match.  If
    // address_prefix_match == true then the query had the EXACT bit false.
    return MATCH_ANY | ADDRESS_ONLY;
  }
  if ((event & EXACT) && !(best_name_match & EXACT)) return 0;
  return best_name_match;
}

// static
openlcb::EventId FindProtocolDefs::input_to_event(const string& input) {
  uint64_t event = TRAIN_FIND_BASE;
  int shift = TRAIN_FIND_MASK - 4;
  unsigned pos = 0;
  bool has_space = true;
  uint32_t qry = 0xFFFFFFFF;
  while (shift >= TRAIN_FIND_MASK_LOW && pos < input.size()) {
    if (is_number(input[pos])) {
      qry <<= 4;
      qry |= input[pos] - '0';
      has_space = false;
      shift -= 4;
    } else {
      if (!has_space) {
        qry <<= 4;
        qry |= 0xF;
        shift -= 4;
      }
      has_space = true;
    }
    pos++;
  }
  event |= uint64_t(qry & 0xFFFFFF) << TRAIN_FIND_MASK_LOW;
  unsigned flags = 0;
  if ((input[0] == '0') || (input.back() == 'L')) {
    flags |= commandstation::DCC_ANY | commandstation::DCC_LONG_ADDRESS;
  } else if (input.back() == 'M') {
    flags |= MARKLIN_NEW;
  } else if (input.back() == 'm') {
    flags |= MARKLIN_OLD;
  } else if (input.back() == 'S') {
    flags |= DCC_ANY;
  }
  event &= ~UINT64_C(0xff);
  event |= (flags & 0xff);

  return event;
}

openlcb::EventId FindProtocolDefs::input_to_search(const string& input) {
  if (input.empty()) {
    return openlcb::TractionDefs::IS_TRAIN_EVENT;
  }
  auto event = input_to_event(input);
  return event;
}

openlcb::EventId FindProtocolDefs::input_to_allocate(const string& input) {
  if (input.empty()) {
    return 0;
  }
  auto event = input_to_event(input);
  event |= (FindProtocolDefs::ALLOCATE | FindProtocolDefs::EXACT |
            FindProtocolDefs::ADDRESS_ONLY);
  return event;
}

uint8_t FindProtocolDefs::match_query_to_train(openlcb::EventId event,
                                               const string& name,
                                               unsigned address, DccMode mode) {
  ExternalTrainDbEntry entry(name, address, mode);
  return match_query_to_node(event, &entry);
}

}  // namespace commandstation
