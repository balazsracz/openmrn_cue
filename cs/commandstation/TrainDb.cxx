#include "commandstation/TrainDb.hxx"

#include <vector>

#include "commandstation/TrainDbCdi.hxx"
#include "openlcb/TractionDefs.hxx"
#include "utils/format_utils.hxx"

namespace commandstation {

TrainDbEntry::~TrainDbEntry() {}

class PtrTrainDbEntry : public TrainDbEntry {
 public:
  /** Retrieves the NMRAnet NodeID for the virtual node that represents a
   * particular train known to the database.
   */
  openlcb::NodeID get_traction_node() override {
    if (entry()->mode & OLCBUSER) {
      return 0x050101010000ULL | static_cast<openlcb::NodeID>(legacy_address());
    } else {
      auto addr = legacy_address();
      return openlcb::TractionDefs::train_node_id_from_legacy(
          dcc_mode_to_address_type(get_legacy_drive_mode(), addr), addr);
    }
  }

  /** Retrieves the name of the train. */
  string get_train_name() override { return entry()->name; }

  /** Retrieves the legacy address of the train. */
  int get_legacy_address() override { return legacy_address(); }

  /** Retrieves the traction drive mode of the train. */
  DccMode get_legacy_drive_mode() override {
    return static_cast<DccMode>(entry()->mode);
  }

  /** Retrieves the label assigned to a given function, or FN_UNUSED if the
      function does not exist. */
  unsigned get_function_label(unsigned fn_id) override {
    if (fn_id >= DCC_MAX_FN) return FN_NONEXISTANT;
    if (fn_id > maxFn_) return FN_NONEXISTANT;
    return entry()->function_labels[fn_id];
  }

  /** Returns the largest valid function ID for this train, or -1 if the train
      has no functions. */
  int get_max_fn() override { return ((int)maxFn_) - 1; }

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
  const const_traindb_entry_t *entry() override {
    return const_lokdb + offset_;
  }
  string identifier() override {
    char buf[10];
    integer_to_buffer(offset_, buf);
    string ret("const/");
    ret.append(buf);
    return ret;
  }

 private:
  uint8_t offset_;  // which offset this entry is in the const_lokdb structure.
};

class ExtPtrTrainDbEntry : public PtrTrainDbEntry {
 public:
  ExtPtrTrainDbEntry(const const_traindb_entry_t *e) : entry_(e) { init(); }
  const const_traindb_entry_t *entry() override { return entry_; }

  string identifier() override {
    char buf[10];
    integer_to_buffer(((intptr_t)entry_) & 0xffffffffu, buf);
    string ret("dyn/");
    ret.append(buf);
    return ret;
  }

 private:
  const const_traindb_entry_t *entry_;
};

class FileTrainDbEntry : public TrainDbEntry {
 public:
  FileTrainDbEntry(int fd, unsigned offset) : cdiEntry_(offset), fd_(fd) {
    init();
  }

  /** Retrieves the NMRAnet NodeID for the virtual node that represents a
   * particular train known to the database.
   */
  openlcb::NodeID get_traction_node() override {
    auto addr = legacy_address();
    return openlcb::TractionDefs::train_node_id_from_legacy(
        dcc_mode_to_address_type(get_legacy_drive_mode(), addr), addr);
  }

  /** Retrieves the name of the train. */
  string get_train_name() override { return cdiEntry_.name().read(fd_); }

  /** Retrieves the legacy address of the train. */
  int get_legacy_address() override { return legacy_address(); }

  /** Retrieves the traction drive mode of the train. */
  DccMode get_legacy_drive_mode() override {
    return static_cast<DccMode>(cdiEntry_.mode().read(fd_));
  }

  /** Retrieves the label assigned to a given function, or FN_NONEXISTANT if
      the function does not exist. */
  unsigned get_function_label(unsigned fn_id) override {
    if (fn_id == 0) return LIGHT;
    if (fn_id >= maxFn_) return FN_NONEXISTANT;
    fn_id--;
    if (fn_id >= cdiEntry_.functions().all_functions().num_repeats()) return FN_NONEXISTANT;
    uint8_t raw_label = cdiEntry_.functions().all_functions().entry(fn_id).icon().read(fd_);
    uint8_t is_mom = cdiEntry_.functions().all_functions().entry(fn_id).is_momentary().read(fd_);
    if (is_mom) raw_label |= 128;
    return raw_label;
  }

  /** Returns the largest valid function ID for this train, or -1 if the train
      has no functions. */
  int get_max_fn() override { return ((int)maxFn_) - 1; }

  string identifier() override {
    char buf[10];
    integer_to_buffer((int)cdiEntry_.offset(), buf);
    string ret("file/");
    ret.append(buf);
    return ret;
  }

  int file_offset() override {
    return cdiEntry_.offset();
  }

 private:
  /** Computes maxFn_. */
  void init() {
    maxFn_ = 1;  // F0 always valid
    for (unsigned i = 0; i < cdiEntry_.functions().all_functions().num_repeats(); ++i) {
      if (cdiEntry_.functions().all_functions().entry(i).icon().read(fd_) !=
          FN_NONEXISTANT) {
        // if entry i valid -> FN(i+1) exists -> maxFn_ == i+2
        maxFn_ = i + 2;
      }
    }
  }

  uint16_t legacy_address() { return cdiEntry_.address().read(fd_); }

  TrainDbCdiEntry cdiEntry_;
  int fd_;
  uint8_t maxFn_;  // Largest valid function ID for this train + 1.
};

std::shared_ptr<TrainDbEntry> create_lokdb_entry(
    const const_traindb_entry_t *e) {
  return std::shared_ptr<TrainDbEntry>(new ExtPtrTrainDbEntry(e));
}

static constexpr unsigned NONEX_OFFSET = 0xDEADBEEF;

TrainDb::TrainDb() : cfg_(NONEX_OFFSET) { init_const_lokdb(); }

TrainDb::TrainDb(const TrainDbConfig cfg)
    : cfg_(cfg.offset()) {
  init_const_lokdb();
}

void TrainDb::init_const_lokdb() {
  for (unsigned i = 0; i < const_lokdb_size; ++i) {
    std::shared_ptr<TrainDbEntry> e(new ConstTrainDbEntry(i));
    if (e->get_legacy_address()) {
      entries_.emplace_back(std::move(e));
    }
  }
}

TrainDb::~TrainDb() {}

/** @return true if this traindb is backed by a file. */
bool TrainDb::has_file() {
  return (cfg_.offset() != NONEX_OFFSET);
}

/** Loads the train database from the given file. The file must stay open so
 * long as *this is alive. */
size_t TrainDb::load_from_file(int fd, bool initial_load) {
  if (cfg_.offset() == NONEX_OFFSET) {
    return 0;
  }
  if (initial_load) {
    for (unsigned i = 0; i < cfg_.num_repeats(); ++i) {
      uint16_t address = cfg_.entry(i).address().read(fd);
      unsigned mode = cfg_.entry(i).mode().read(fd);
      if (address != 0 && address != 0xffffu && mode != 0) {
        entries_.emplace_back(new FileTrainDbEntry(fd, cfg_.entry(i).offset()));
      }
    }
  } else {
    for (unsigned i = 0; i < cfg_.num_repeats(); ++i) {
      uint16_t address = cfg_.entry(i).address().read(fd);
      if (address != 0 && address != 0xffffu) {
        std::shared_ptr<FileTrainDbEntry> e(new FileTrainDbEntry(fd, cfg_.entry(i).offset()));
        openlcb::NodeID traction_node = e->get_traction_node();
        bool found = false;
        for (unsigned i = 0; i < entries_.size(); ++i) {
          if (entries_[i]->get_traction_node() == traction_node) {
            entries_[i] = e;
            found = true;
          }
        }
        if (!found) {
          entries_.emplace_back(std::move(e));
        }
      }
    }
  }
  return cfg_.end_offset();
}

}  // namespace commandstation
