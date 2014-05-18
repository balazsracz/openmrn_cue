#include "mobilestation/TrainDb.hxx"

#include "nmranet/TractionDefs.hxx"

namespace mobilestation {

bool TrainDb::is_train_id_known(unsigned train_id) {
  return train_id < const_lokdb_size;
}

NMRAnet::NodeID TrainDb::get_traction_node(unsigned train_id) {
  HASSERT(is_train_id_known(train_id));
  const struct const_loco_db_t *entry = const_lokdb + train_id;
  return NMRAnet::TractionDefs::NODE_ID_DCC | entry->address;
}

unsigned TrainDb::get_function_address(unsigned train_id, unsigned fn_id) {
  HASSERT(is_train_id_known(train_id));
  const struct const_loco_db_t *entry = const_lokdb + train_id;
  if (fn_id < 2) return UNKNOWN_FUNCTION;
  fn_id -= 2;
  if (fn_id >= DCC_MAX_FN) return UNKNOWN_FUNCTION;
  if (entry->function_mapping[fn_id] == 0xff) return UNKNOWN_FUNCTION;
  return entry->function_mapping[fn_id];
}

unsigned TrainDb::get_function_label(unsigned train_id, unsigned fn_id) {
  HASSERT(is_train_id_known(train_id));
  const struct const_loco_db_t *entry = const_lokdb + train_id;
  if (fn_id < 2) return UNKNOWN_FUNCTION;
  fn_id -= 2;
  if (fn_id >= DCC_MAX_FN) return UNKNOWN_FUNCTION;
  if (entry->function_labels[fn_id] == 0xff) return UNKNOWN_FUNCTION;
  return entry->function_labels[fn_id];
}

}  // namespace mobilestation
