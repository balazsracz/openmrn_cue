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
 * \file TrainDb.hxx
 *
 * Interface for accessing a train database for the mobile station lookup.
 *
 * @author Balazs Racz
 * @date 18 May 2014
 */

#include "nmranet/NMRAnetIf.hxx"

namespace mobilestation {

#define DCC_MAX_FN 22

struct const_loco_db_t {
  const uint8_t address;
  // Maps the logical function numbers ot F bits. 0xff terminates the array.
  const uint8_t function_mapping[DCC_MAX_FN];
  // MoSta function definition, parallel to function_mapping.
  const uint8_t function_labels[DCC_MAX_FN];
  // Any zero character ends, but preferable to pad it with zeroes.
  const char name[16];
  const uint8_t mode;
};

extern const struct const_loco_db_t const_lokdb[];
extern const size_t const_lokdb_size;

enum Symbols {
  LIGHT = 1,
  BEAMER = 2,
  TELEX = 6,
  ABV = 8,
  SMOKE = 10,
  FNT11 = 11,
  FNT12 = 12,
  ENGINE = 13,
  LIGHT1 = 14,
  LIGHT2 = 15,
  HONK = 132,
  WHISTLE = 133,
  FNP = 139,
  SPEECH = 140,
  SOUNDP = 141
};

enum DccMode {
  MARKLIN_OLD = 0,
  MARKLIN_NEW = 1,
  MFX = 2,
  DCC_14 = 4,
  DCC_28 = 5,
  DCC_128 = 6,
  PUSHPULL = 8,
  MARKLIN_TWOADDR = 16
};

class TrainDb {
 public:
  /** Returns true if a train of a specific identifier is known to the
   * traindb.
   * @param id is the train identifier. Valid values: anything. Typical values:
   * 0..NUM_TRAINS*/
  bool is_train_id_known(unsigned train_id);

  /** Retrieves the NMRAnet NodeID for the virtual node that represents a
   * particular train known to the database.
   *
   * Requires: is_train_id_known(train_id) == true. */
  nmranet::NodeID get_traction_node(unsigned train_id);

  enum {
    UNKNOWN_FUNCTION = 0xFFFFFFFF,
  };

  /** Retrieves the address of a function mapped to a specific fn_id.
   *
   * @param fn_id is a (dense) function identifier, starting at 2, of assigned
   * functions.
   * @returns an NMRAnet function space address, typically in the range 0..28,
   * to be used for direct assignment of this function bit. Returns
   * UNKNOWN_FUNCTION if the function does not exist. */
  unsigned get_function_address(unsigned train_id, unsigned fn_id);

  /** Retrieves the label of an assigned function.
   *
   * @param fn_id is a (dense) function identifier, starting at 2, of assigned
   * functions.
   * @returns an enum FunctionLabel, as used by the marklin mobile station. */
  unsigned get_function_label(unsigned train_id, unsigned fn_id);

  /** Returns the dcc DriveMode for the given train id.
   *
   * @param train_id must be a valid train id.
   * @returns a DccMode enum value -- low 3 bits are an enum, bit 3 and 4 are
   * bitmasks. */
  unsigned get_drive_mode(unsigned train_id);
};

}  // namespace mobilestation
