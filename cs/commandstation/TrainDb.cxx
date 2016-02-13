#include "commandstation/TrainDb.hxx"

#include <vector>

#include "commandstation/TrainDbCdi.hxx"
#include "nmranet/TractionDefs.hxx"
#include "utils/format_utils.hxx"

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
  virtual DccMode get_legacy_drive_mode() {
    return static_cast<DccMode>(entry()->mode);
  }

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
  virtual const const_traindb_entry_t *entry() { return entry_; }

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
  nmranet::NodeID get_traction_node() override {
    return nmranet::TractionDefs::NODE_ID_DCC |
           static_cast<nmranet::NodeID>(legacy_address());
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
    if (fn_id >= cdiEntry_.all_functions().num_repeats()) return FN_NONEXISTANT;
    if (fn_id >= maxFn_) return FN_NONEXISTANT;
    return cdiEntry_.all_functions().entry(fn_id).icon().read(fd_);
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

 private:
  /** Computes maxFn_. */
  void init() {
    maxFn_ = 0;
    for (unsigned i = 0; i < cdiEntry_.all_functions().num_repeats(); ++i) {
      if (cdiEntry_.all_functions().entry(i).icon().read(fd_) !=
          FN_NONEXISTANT) {
        maxFn_ = i + 1;
      }
    }
  }

  uint16_t legacy_address() { return cdiEntry_.address().read(fd_); }

  TrainDbCdiEntry cdiEntry_;
  int fd_;
  uint8_t maxFn_;  // Largest valid function ID for this train.
};

std::shared_ptr<TrainDbEntry> create_lokdb_entry(
    const const_traindb_entry_t *e) {
  return std::shared_ptr<TrainDbEntry>(new ExtPtrTrainDbEntry(e));
}

class TrainDb::TrainDbUpdater : private DefaultConfigUpdateListener {
 public:
  TrainDbUpdater(TrainDb *parent) : parent_(parent) {}

 private:
  // ConfigUpdate interface
  UpdateAction apply_configuration(int fd, bool initial_load,
                                   BarrierNotifiable *done) override;

  void factory_reset(int fd) override;

  TrainDb *parent_;
};

static constexpr unsigned NONEX_OFFSET = 0xDEADBEEF;

TrainDb::TrainDb() : cfg_(NONEX_OFFSET) { init_const_lokdb(); }

TrainDb::TrainDb(const TrainDbConfig cfg)
    : updater_(new TrainDb::TrainDbUpdater(this)), cfg_(cfg.offset()) {
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

ConfigUpdateListener::UpdateAction TrainDb::TrainDbUpdater::apply_configuration(
    int fd, bool initial_load, BarrierNotifiable *done) {
  auto &cfg = parent_->cfg_;
  if (cfg.offset() == NONEX_OFFSET) {
    return UPDATED;
  }
  if (initial_load) {
    for (unsigned i = 0; i < cfg.num_repeats(); ++i) {
      if (cfg.entry(i).address().read(fd) != 0) {
        parent_->entries_.emplace_back(
            new FileTrainDbEntry(fd, cfg.entry(i).offset()));
      }
    }
  }
  return UPDATED;
}

void TrainDb::TrainDbUpdater::factory_reset(int fd) {}

}  // namespace commandstation
