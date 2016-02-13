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

#ifndef _MOBILESTATION_TRAINDB_HXX_
#define _MOBILESTATION_TRAINDB_HXX_

#include <memory>
#include "nmranet/Defs.hxx"
#include "utils/ConfigUpdateListener.hxx"
#include "commandstation/TrainDbDefs.hxx"
#include "commandstation/TrainDbCdi.hxx"

namespace commandstation {

struct const_traindb_entry_t {
  const uint16_t address;
  // MoSta function definition, parallel to function_mapping.
  const uint8_t function_labels[DCC_MAX_FN];
  // Any zero character ends, but preferable to pad it with zeroes.
  const char name[16];
  const uint8_t mode;
};

extern const struct const_traindb_entry_t const_lokdb[];
extern const size_t const_lokdb_size;


class TrainDbEntry {
public:
  virtual ~TrainDbEntry();

  /** Returns an internal identifier that uniquely defines where this traindb
   * entry was allocated from. */
  virtual string identifier() = 0;

  /** Retrieves the NMRAnet NodeID for the virtual node that represents a
   * particular train known to the database.
   */
  virtual nmranet::NodeID get_traction_node() = 0;

  /** Retrieves the name of the train. */
  virtual string get_train_name() = 0;

  /** Retrieves the legacy address of the train. */
  virtual int get_legacy_address() = 0;

  /** Retrieves the traction drive mode of the train. */
  virtual DccMode get_legacy_drive_mode() = 0;

  /** Retrieves the label assigned to a given function, or FN_NONEXISTANT if
      the function does not exist. */
  virtual unsigned get_function_label(unsigned fn_id) = 0;

  /** Returns the largest valid function ID for this train, or -1 if the train
      has no functions. */
  virtual int get_max_fn() = 0;
};

/// Used for testing code that depends on traindb entries.
std::shared_ptr<TrainDbEntry> create_lokdb_entry(
    const const_traindb_entry_t* e);

class TrainDb {
 public:
  /** Use this constructor if there is no file-based train database. */
  TrainDb();
  /** Use this constructor when there is a train DB in the CDI. */
  TrainDb(const TrainDbConfig cfg);
  ~TrainDb();

  /** Returns true if a train of a specific identifier is known to the
   * traindb.
   * @param id is the train identifier. Valid values: anything. Typical values:
   * 0..NUM_TRAINS*/
  bool is_train_id_known(unsigned train_id) {
    return train_id < entries_.size();
  }

  /** Returns a train DB entry if the train ID is known, otherwise nullptr. The
      ownership of the entry is not transferred. */
  std::shared_ptr<TrainDbEntry> get_entry(unsigned train_id) {
    if (train_id < entries_.size()) return entries_[train_id];
    return nullptr;
  }

  /** Inserts a given entry into the train database. @param entry is the new
      traindsb entry. Transfers ownership to the TrainDb class. @returns the
      new train_id for the given entry. */
  unsigned add_dynamic_entry(TrainDbEntry* entry) {
    unsigned s = entries_.size();
    std::shared_ptr<TrainDbEntry> e(entry);
    entries_.push_back(e);
    return s;
  }

private:
  /** Creates all entries for the compiled-in train database. */
  void init_const_lokdb();

  /// This class is used when there is a config file and we shall be listening
  /// to the changes in it.
  class TrainDbUpdater;
  friend class TrainDbUpdater;
  std::unique_ptr<TrainDbUpdater> updater_;
  TrainDbConfig cfg_;
  vector<std::shared_ptr<TrainDbEntry> > entries_;
};

}  // namespace commandstation

#endif // _MOBILESTATION_TRAINDB_HXX_
