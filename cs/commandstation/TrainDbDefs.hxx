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
 * \file TrainDbDefs.hxx
 *
 * Common definitions for the train database.
 *
 * @author Balazs Racz
 * @date 13 Feb 2016
 */

#ifndef _COMMANDSTATION_TRAINDBDEFS_HXX_
#define _COMMANDSTATION_TRAINDBDEFS_HXX_

#include "dcc/Defs.hxx"

namespace commandstation {

#define DCC_MAX_FN 29


enum Symbols {
  FN_NONEXISTANT = 0,
  LIGHT = 1,
  BEAMER = 2,
  BELL = 3,
  HORN = 128 + 4,
  SHUNT = 5,
  PANTO = 6,
  SMOKE = 7,
  ABV = 8,
  WHISTLE = 128 + 9,
  SOUND = 10,
  FNT11 = 11,
  SPEECH = 128 + 12,
  ENGINE = 13,
  LIGHT1 = 14,
  LIGHT2 = 15,
  TELEX = 128 + 17,
  FN_UNKNOWN = 127,
  MOMENTARY = 128,
  FNP = 139,
  SOUNDP = 141,
  // Empty eeprom will have these bytes.
  FN_UNINITIALIZED = 255,
};

enum DccMode {
  FAKE_DRIVE = 7,

  MARKLIN_OLD = 1,
  MARKLIN_NEW = 2,
  MFX = 3,

  DCC_14 = 4,
  DCC_28 = 5,
  DCC_128 = 6,

  /// Bit mask for the protocol field only.
  PROTOCOL_MASK = 7,

  DCC_ANY = 4,
  PUSHPULL = 8,
  MARKLIN_TWOADDR = 16,
  OLCBUSER = 32,
  DCC_LONG_ADDRESS = 64,

  DCC_14_LONG_ADDRESS = 68,
  DCC_28_LONG_ADDRESS = 69,
  DCC_128_LONG_ADDRESS = 70,
};

inline dcc::TrainAddressType dcc_mode_to_address_type(DccMode mode,
                                                      uint32_t address) {
  if ((mode == MARKLIN_OLD || mode == MARKLIN_NEW)) {
    return dcc::TrainAddressType::MM;
  }
  if ((mode & DCC_LONG_ADDRESS) || (address >= 128)) {
    return dcc::TrainAddressType::DCC_LONG_ADDRESS;
  }
  return dcc::TrainAddressType::DCC_SHORT_ADDRESS;
}

} // namespace commandstation

#endif // _COMMANDSTATION_TRAINDBDEFS_HXX_
