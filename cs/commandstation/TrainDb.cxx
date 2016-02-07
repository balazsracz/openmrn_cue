#include "commandstation/TrainDb.hxx"

#include <vector>

#include "nmranet/TractionDefs.hxx"

namespace commandstation {

static vector<uint8_t> compute_num_fn_per_train() {
  vector<uint8_t> ret;
  ret.reserve(const_lokdb_size);
  for (unsigned i = 0; i < const_lokdb_size; ++i) {
    const struct const_loco_db_t *entry = const_lokdb + i;
    int n = 0;
    if (entry->address) {
      while (n < DCC_MAX_FN && entry->function_mapping[n] != 0xff &&
             entry->function_labels[n] != 0xff)
        ++n;
      HASSERT(n < DCC_MAX_FN && entry->function_mapping[n] == 0xff &&
              entry->function_labels[n] == 0xff);
    }
    ret.push_back(n);
  }
  return ret;
}

static const vector<uint8_t> g_num_fn_per_train = compute_num_fn_per_train();

bool TrainDb::is_train_id_known(unsigned train_id) {
  return train_id < const_lokdb_size && (const_lokdb[train_id].address > 0);
}

const char* TrainDb::get_train_name(unsigned train_id) {
  HASSERT(is_train_id_known(train_id));
  const struct const_loco_db_t *entry = const_lokdb + train_id;
  return entry->name;
}

int TrainDb::get_legacy_address(unsigned train_id) {
  HASSERT(is_train_id_known(train_id));
  const struct const_loco_db_t *entry = const_lokdb + train_id;
  return entry->address;
}

DccMode TrainDb::get_legacy_drive_mode(unsigned train_id) {
  HASSERT(is_train_id_known(train_id));
  const struct const_loco_db_t *entry = const_lokdb + train_id;
  return static_cast<DccMode>(entry->mode);
}

nmranet::NodeID TrainDb::get_traction_node(unsigned train_id) {
  HASSERT(is_train_id_known(train_id));
  const struct const_loco_db_t *entry = const_lokdb + train_id;
  if (entry->mode & OLCBUSER) {
    return 0x050101010000ULL | static_cast<nmranet::NodeID>(entry->address);
  } else {
    return nmranet::TractionDefs::NODE_ID_DCC | static_cast<nmranet::NodeID>(entry->address);
  }
}

unsigned TrainDb::get_function_address(unsigned train_id, unsigned fn_id) {
  HASSERT(is_train_id_known(train_id));
  const struct const_loco_db_t *entry = const_lokdb + train_id;
  if (fn_id < 2) return UNKNOWN_FUNCTION;
  fn_id -= 2;
  // Pause and reverse should be just above the dcc function space.
  if (fn_id == 30) return 29;
  if (fn_id == 31) return 30;
  if (fn_id >= DCC_MAX_FN) return UNKNOWN_FUNCTION;
  if (fn_id >= g_num_fn_per_train[train_id]) return UNKNOWN_FUNCTION;
  if (entry->function_mapping[fn_id] == 0xff) return UNKNOWN_FUNCTION;
  return entry->function_mapping[fn_id];
}

unsigned TrainDb::get_function_label(unsigned train_id, unsigned fn_id) {
  HASSERT(is_train_id_known(train_id));
  const struct const_loco_db_t *entry = const_lokdb + train_id;
  if (fn_id < 2) return UNKNOWN_FUNCTION;
  fn_id -= 2;
  if (fn_id >= DCC_MAX_FN) return UNKNOWN_FUNCTION;
  if (fn_id >= g_num_fn_per_train[train_id]) return UNKNOWN_FUNCTION;
  if (entry->function_labels[fn_id] == 0xff) return UNKNOWN_FUNCTION;
  return entry->function_labels[fn_id];
}

DccMode TrainDb::get_drive_mode(unsigned train_id) {
  HASSERT(is_train_id_known(train_id));
  const struct const_loco_db_t *entry = const_lokdb + train_id;
  return static_cast<DccMode>(entry->mode);
}

}  // namespace commandstation
