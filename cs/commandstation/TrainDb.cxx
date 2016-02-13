#include "commandstation/TrainDb.hxx"

#include <vector>

#include "nmranet/TractionDefs.hxx"
#include "commandstation/TrainDbCdi.hxx"


namespace commandstation {

TrainDbEntry::~TrainDbEntry() {}

class PtrTrainDbEntry : public TrainDbEntry {
 public:
  /** Retrieves the NMRAnet NodeID for the virtual node that represents a
   * particular train known to the database.
   */
  virtual nmranet::NodeID get_traction_node() {
    if (entry()->mode & OLCBUSER) {
      return 0x050101010000ULL | static_cast<nmranet::NodeID>(legacy_address());
    } else {
      return nmranet::TractionDefs::NODE_ID_DCC |
             static_cast<nmranet::NodeID>(legacy_address());
    }
  }

  /** Retrieves the name of the train. */
  virtual string get_train_name() { return entry()->name; }

  /** Retrieves the legacy address of the train. */
  virtual int get_legacy_address() { return legacy_address(); }

  /** Retrieves the traction drive mode of the train. */
  virtual DccMode get_legacy_drive_mode() { return static_cast<DccMode>(entry()->mode); }

  /** Retrieves the label assigned to a given function, or FN_UNUSED if the
      function does not exist. */
  virtual unsigned get_function_label(unsigned fn_id) {
    if (fn_id >= DCC_MAX_FN) return FN_NONEXISTANT;
    if (fn_id > maxFn_) return FN_NONEXISTANT;
    return entry()->function_labels[fn_id];
  }

  /** Returns the largest valid function ID for this train, or -1 if the train
      has no functions. */
  virtual int get_max_fn() { return ((int)maxFn_) - 1; }

 protected:
  /** Child classes must call tis once after creation. */
  void init() {
    maxFn_ = 0;
    for (int i = 0; i < DCC_MAX_FN; ++i) {
      if (entry()->function_labels[i] != FN_NONEXISTANT) {
        maxFn_ = i + 1;
      }
    }
  }

  virtual const const_traindb_entry_t *entry() = 0;

 private:
  uint16_t legacy_address() { return entry()->address; }

  uint8_t maxFn_;  // Largest valid function ID for this train.
};

class ConstTrainDbEntry : public PtrTrainDbEntry {
 public:
  ConstTrainDbEntry(uint8_t offset) : offset_(offset) { init(); }
  virtual const const_traindb_entry_t *entry() { return const_lokdb + offset_; }

 private:
  uint8_t offset_;  // which offset this entry is in the const_lokdb structure.
};

class ExtPtrTrainDbEntry : public PtrTrainDbEntry {
 public:
  ExtPtrTrainDbEntry(const const_traindb_entry_t* e) : entry_(e) { init(); }
  virtual const const_traindb_entry_t *entry() { return entry_; }

 private:
  const const_traindb_entry_t *entry_;
};


class FileTrainDbEntry : public TrainDbEntry {
 public:
  /** Retrieves the NMRAnet NodeID for the virtual node that represents a
   * particular train known to the database.
   */
  virtual nmranet::NodeID get_traction_node() {
    return nmranet::TractionDefs::NODE_ID_DCC |
      static_cast<nmranet::NodeID>(legacy_address());
  }

  /** Retrieves the name of the train. */
  virtual string get_train_name() { return entry()->name; }

  /** Retrieves the legacy address of the train. */
  virtual int get_legacy_address() { return legacy_address(); }

  /** Retrieves the traction drive mode of the train. */
  virtual DccMode get_legacy_drive_mode() { return static_cast<DccMode>(entry()->mode); }

  /** Retrieves the label assigned to a given function, or FN_UNUSED if the
      function does not exist. */
  virtual unsigned get_function_label(unsigned fn_id) {
    if (fn_id >= DCC_MAX_FN) return FN_NONEXISTANT;
    if (fn_id > maxFn_) return FN_NONEXISTANT;
    return entry()->function_labels[fn_id];
  }

  /** Returns the largest valid function ID for this train, or -1 if the train
      has no functions. */
  virtual int get_max_fn() { return ((int)maxFn_) - 1; }

 protected:
  /** Child classes must call tis once after creation. */
  void init() {
    maxFn_ = 0;
    for (int i = 0; i < cdiEntry_.all_functions().num_repeats(); ++i) {
      if (cdiEntry_.all_functions().entry(i).icon().read(fd_) !=
          FN_NONEXISTANT) {
        maxFn_ = i + 1;
      }
    }
  }

 private:
  uint16_t legacy_address() { return cdiEntry_.address().read(fd_); }

  TrainDbCdiEntry cdiEntry_;
  int fd_;
  uint8_t maxFn_;  // Largest valid function ID for this train.
};


std::shared_ptr<TrainDbEntry> create_lokdb_entry(const const_traindb_entry_t* e) {
  return std::shared_ptr<TrainDbEntry>(new ExtPtrTrainDbEntry(e));
}


TrainDb::TrainDb() {
  for (unsigned i = 0; i < const_lokdb_size; ++i) {
    std::shared_ptr<TrainDbEntry> e(new ConstTrainDbEntry(i));
    if (e->get_legacy_address()) {
      entries_.emplace_back(std::move(e));
    }
  }
}

TrainDb::~TrainDb() {}

}  // namespace commandstation
