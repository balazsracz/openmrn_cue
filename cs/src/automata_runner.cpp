#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#define LOGLEVEL WARNING
// Workaroundfor bug in <memory> for gcc 2.6.2 lpcxpresso newlib
#ifndef __CR2_C___4_6_2_BITS_SHARED_PTR_H__
#define __CR2_C___4_6_2_BITS_SHARED_PTR_H__
#endif

#include <stdio.h>
#include <unistd.h>

#include <memory>

#include "utils/macros.h"
//#include "core/nmranet_event.h"

//#include "common_event_handlers.hxx"

#include "automata_defs.h"
#include "automata_runner.h"
#include "automata_control.h"
#include "dcc-master.h"

#include "utils/logging.h"
#include "openlcb/Defs.hxx"
#include "openlcb/If.hxx"
#include "openlcb/WriteHelper.hxx"
#include "openlcb/EventHandlerTemplates.hxx"
#include "openlcb/EventService.hxx"
#include "openlcb/TractionDefs.hxx"

extern int debug_variables;
int debug_variables __attribute__((weak)) = 0;

// This write helper will only ever be used synchronously.
static openlcb::WriteHelper automata_write_helper;
AutomataDebugSpace g_aut_debug_space;
static SyncNotifiable g_notify_wait;
static BarrierNotifiable g_barrier_notify;

static BarrierNotifiable* get_notifiable() {
  return g_barrier_notify.reset(&g_notify_wait);
}

static void wait_for_notification() { g_notify_wait.wait_for_notification(); }

/*
  TODOS:

  somehow define the timer handling routine. Maybe set a particular bit to be
  the timer bit. Write a class TimerBit and auto-populate it during import?

  need to replace IF_ALIVE with something.

  need to implement automata running controls.

  should add a mutex for running automata.

  need to create the automatas.

  need a destructor for AutomataRunner.
 */

void UpdateSlaves(void);

AutomataRunner& AutomataRunner::ResetForAutomata(Automata* aut) {
  ASSERT(aut);
  current_automata_ = aut;
  ip_ = aut->GetStartingOffset();
  memset(imported_bits_, 0, sizeof(imported_bits_));
  memset(imported_bit_args_, 0, sizeof(imported_bit_args_));
  imported_bits_[0] = aut->GetTimerBit();
  return *this;
}

void AutomataRunner::CreateVarzAndAutomatas() {
  ip_ = 0;
  int id = 0;
  aut_offset_t last_ofs = ip_;
  aut_offset_t last_raw_ofs = ip_;
  do {
    aut_offset_t ofs = load_insn();
    ofs |= load_insn() << 8;
    LOG(VERBOSE, "read automata ofs: ofs %d\n", ofs);
    if (!ofs) break;
    aut_offset_t final_ofs = ofs;
    if (ofs < last_raw_ofs) {
      final_ofs = ((last_ofs & ~0xffff) + 0x10000) | ofs;
    } else {
      final_ofs = (last_ofs & ~0xffff) | ofs;
    }
    last_raw_ofs = ofs;
    all_automata_.push_back(new ::Automata(id++, final_ofs));
    LOG(WARNING, "automata %d raw_ofs %d final_ofs: ofs %d\n", id - 1, ofs,
        final_ofs);
    last_ofs = final_ofs;
  } while (1);
  // This will execute all preamble commands, including the variable create
  // commands.
  Run();
}

void AutomataRunner::debug_hook() {
  auto print_ip = last_ip_;
  last_ip_ = ip_;
  if (!g_aut_debug_space.logEventId_) return;
  if (automata_write_helper.last_payload().size() != 8) return;
  if (*(uint64_t*)(automata_write_helper.last_payload().data()) ==
      g_aut_debug_space.logEventId_) {
    g_aut_debug_space.ip_ = htobe16(print_ip);
  }
  automata_write_helper.clear_last_payload();
}

void AutomataRunner::Run() {
  uint8_t insn;
  uint8_t numif, numact;
  aut_offset_t endif, endcond;
  bool keep;
  while (1) {
    // LOG(VERBOSE, "ip: %d\n", ip_);
    insn = load_insn();
    if (!insn) break;

    //    while((insn = load_insn()) != 0) {
    numif = insn & 0x0f;
    numact = insn >> 4;
    endif = ip_ + numif;
    endcond = endif + numact;
    keep = true;
    insn_t insn, arg;
    while (ip_ < endif) {
      debug_hook();
      insn = load_insn();
      if ((insn & _IF_MISCA_MASK) == _IF_MISCA_BASE) {
        if (ip_ >= endif) {
          diewith(CS_DIE_AUT_TWOBYTEFAIL);
        }
        arg = load_insn();
        if (!eval_condition2(insn, arg)) {
          keep = false;
          break;
        }
      } else if (!eval_condition(insn)) {
        keep = false;
        break;
      }
    }
    if (!keep) {
      ip_ = endcond;
      continue;
    }
    while (ip_ < endcond) {
      debug_hook();
      insn = load_insn();
      if (insn == _ACT_IMPORT_VAR) {
        import_variable();
        if (ip_ > endcond) {
          diewith(CS_DIE_AUT_TWOBYTEFAIL);
        }
      } else if (insn == _ACT_SET_EVENTID) {
        insn_load_event_id();
        if (ip_ > endcond) {
          diewith(CS_DIE_AUT_TWOBYTEFAIL);
        }
      } else if (insn == _ACT_DEF_VAR) {
        int offset = ip_;
        ReadWriteBit* newbit = create_variable();
        if (ip_ > endcond) {
          diewith(CS_DIE_AUT_TWOBYTEFAIL);
        }
        delete declared_bits_[offset];
        declared_bits_[offset] = newbit;
      } else if (insn == _ACT_SET_VAR_VALUE) {
        arg = load_insn();
        int offset = arg >> 5;
        int var = arg & 31;
        arg = load_insn();
        if (ip_ > endcond) {
          diewith(CS_DIE_AUT_TWOBYTEFAIL);
        }
        GetBit(var)->SetState(offset, arg);
      } else if ((insn & _ACT_MISCA_MASK) == _ACT_MISCA_BASE) {
        if (ip_ >= endcond) {
          diewith(CS_DIE_AUT_TWOBYTEFAIL);
        }
        arg = load_insn();
        eval_action2(insn, arg);
      } else {
        eval_action(insn);
      }
    }
    debug_hook();
  }
}

void AutomataRunner::import_variable() {
  uint8_t local_idx = load_insn();
  uint16_t arg = local_idx >> 5;
  local_idx &= 31;
  // Need to rewrite the bit fields if we want a different local variable set
  // size.
  HASSERT(32 == (sizeof(imported_bits_) / sizeof(imported_bits_[0])));
  arg <<= 8;
  arg |= load_insn();
  uint16_t global_ofs = load_insn();
  global_ofs |= load_insn() << 8;
  if (local_idx >= (sizeof(imported_bits_) / sizeof(imported_bits_[0]))) {
    // The local variable offset is out of bounds.
    diewith(CS_DIE_AUT_HALT);
  }
  auto it = declared_bits_.find(global_ofs);
  if (it == declared_bits_.end()) {
    // The global variable that it refers to is not known.
    diewith(CS_DIE_AUT_HALT);
  }
  imported_bits_[local_idx] = it->second;
  imported_bit_args_[local_idx] = arg;
}

class EventBit : public ReadWriteBit {
 public:
  EventBit(openlcb::Node* node, uint64_t event_on, uint64_t event_off,
           uint8_t mask, uint8_t* ptr)
      : bit_(node, event_on, event_off, ptr, mask),
        pc_(&bit_),
        defined_(false) {
    if (0) fprintf(stderr, "event bit create on node %p\n", node);
  }

  void Initialize(openlcb::Node*) override {
    pc_.SendQuery(&automata_write_helper, get_notifiable());
    wait_for_notification();
  }

  bool Read(uint16_t, openlcb::Node*, Automata* aut) override {
    // TODO(balazs.racz): we should consider CHECK failing here if
    // !defined. That will force us to explicitly reset every bit in StInit.
    auto state = bit_.get_current_state();
    if (state == openlcb::EventState::VALID) return true;
    if (state == openlcb::EventState::INVALID) return false;
    LOG_ERROR("Reading event bit of invalid state");
    return false;
  }

  void Write(uint16_t, openlcb::Node* node, Automata* aut, bool value) override {
    if (0) fprintf(stderr, "event bit write to node %p\n", node);
    auto state = bit_.get_current_state();
    if (state == openlcb::EventState::VALID && defined_ && value) return;
    if (state == openlcb::EventState::INVALID && defined_ && !value) return;
    bit_.set_state(value);
    pc_.SendEventReport(&automata_write_helper, get_notifiable());
    wait_for_notification();
    defined_ = true;
  }

 private:
  openlcb::MemoryBit<uint8_t> bit_;
  openlcb::BitEventPC pc_;
  // This bit is true if we've already seen an event that defines this bit.
  bool defined_;
};

class EventBlockBit : public ReadWriteBit {
 public:
  EventBlockBit(openlcb::WriteHelper::node_type node, uint64_t event_base,
                size_t size)
      : storage_(new uint32_t[(size + 31) >> 5]),
        handler_(
            new openlcb::BitRangeEventPC(node, event_base, storage_, size)) {
    size_t sz = (size + 31) >> 5;
    memset(&storage_[0], 0, sz * sizeof(storage_[0]));
  }

  ~EventBlockBit() { delete[] storage_; }

  bool Read(uint16_t arg, openlcb::Node*, Automata* aut) override {
    return handler_->Get(arg);
  }

  void Write(uint16_t arg, openlcb::Node*, Automata* aut, bool value) override {
    handler_->Set(arg, value, &automata_write_helper, get_notifiable());
    wait_for_notification();
  }

  void Initialize(openlcb::Node*) OVERRIDE {
    handler_->SendIdentified(&automata_write_helper, get_notifiable());
    wait_for_notification();
  }

 private:
  uint32_t* storage_;
  std::unique_ptr<openlcb::BitRangeEventPC> handler_;
};

class EventByteBlock : public ReadWriteBit {
 public:
  EventByteBlock(openlcb::WriteHelper::node_type node, uint64_t event_base,
                 size_t size)
      : storage_(new uint8_t[size]),
        handler_(
            new openlcb::ByteRangeEventP(node, event_base, storage_, size)) {
    memset(&storage_[0], 0, size);
  }

  ~EventByteBlock() { delete[] storage_; }

  bool Read(uint16_t arg, openlcb::Node*, Automata* aut) override { HASSERT(0); }

  void Write(uint16_t arg, openlcb::Node*, Automata* aut, bool value) override {
    HASSERT(0);
  }

  uint8_t GetState(uint16_t arg) override { return storage_[arg]; }

  void SetState(uint16_t arg, uint8_t state) override {
    if (storage_[arg] != state) {
      storage_[arg] = state;
      handler_->Update(arg, &automata_write_helper, get_notifiable());
      wait_for_notification();
    }
  }

  void Initialize(openlcb::Node*) OVERRIDE {
    handler_->SendIdentified(&automata_write_helper, get_notifiable());
    wait_for_notification();
  }

 private:
  uint8_t* storage_;
  std::unique_ptr<openlcb::ByteRangeEventP> handler_;
};

class EventByteBlockConsumer : public ReadWriteBit {
 public:
  EventByteBlockConsumer(openlcb::WriteHelper::node_type node,
                         uint64_t event_base, size_t size)
      : storage_(new uint8_t[size]),
        handler_(
            new openlcb::ByteRangeEventC(node, event_base, storage_, size)) {
    memset(&storage_[0], 0, size);
  }

  ~EventByteBlockConsumer() { delete[] storage_; }

  bool Read(uint16_t arg, openlcb::Node*, Automata* aut) override { HASSERT(0); }

  void Write(uint16_t arg, openlcb::Node*, Automata* aut, bool value) override {
    HASSERT(0);
  }

  uint8_t GetState(uint16_t arg) override { return storage_[arg]; }

  void SetState(uint16_t arg, uint8_t state) override {
    storage_[arg] = state;
  }

  void Initialize(openlcb::Node*) OVERRIDE {
    handler_->SendIdentified(&automata_write_helper, get_notifiable());
    wait_for_notification();
  }

 private:
  uint8_t* storage_;
  std::unique_ptr<openlcb::ByteRangeEventC> handler_;
};

ReadWriteBit* AutomataRunner::create_variable() {
  uint8_t arg1 = load_insn();
  uint8_t arg2 = load_insn();
  int type = arg1 >> 5;
  int client = arg1 & 0b11111;
  int offset = arg2 >> 3;
  int bit = arg2 & 7;
  uint8_t* ptr = get_state_byte(client, offset);
  switch (type) {
    case 0:
      return new EventBit(openmrn_node_, aut_eventids_[1], aut_eventids_[0],
                          (1 << bit), ptr);
    case 1: {
      uint16_t size = ((client & 7) << 8) | arg2;
      return new EventBlockBit(openmrn_node_, aut_eventids_[0], size);
    }
    case 2: {
      return new EventByteBlock(openmrn_node_, aut_eventids_[0], arg2);
    }
    case 3: {
      return new EventByteBlockConsumer(openmrn_node_, aut_eventids_[0], arg2);
    }
    default:
      diewith(CS_DIE_UNSUPPORTED);
  }  // switch type
  return NULL;
}

void AutomataRunner::InjectBit(aut_offset_t offset, ReadWriteBit* bit) {
  delete declared_bits_[offset];
  declared_bits_[offset] = bit;
}

void AutomataRunner::insn_load_event_id() {
  uint8_t type = load_insn();
  int dest = (type >> 6) & 1;
  int src = (type >> 4) & 1;
  aut_eventids_[dest] = aut_eventids_[src];
  int ofs = (type & 7) * 8;
  while (1) {
    aut_eventids_[dest] &= ~(0xffULL << ofs);
    aut_eventids_[dest] |= (uint64_t(load_insn()) << ofs);
    if (!ofs) break;
    ofs -= 8;
  }
}

class LockBit : public ReadWriteBit {
 public:
  LockBit(unsigned offset) : lock_id_(offset) { ASSERT(offset < MAX_LOCK_ID); }
  virtual ~LockBit();

  bool Read(uint16_t, openlcb::Node* node, Automata* aut) override {
    int existing_id = locks[lock_id_];
    if (!existing_id) return false;
    if (existing_id == aut->GetId()) return false;
    return true;
  }

  void Write(uint16_t, openlcb::Node* node, Automata* aut, bool value) override {
    if (locks[lock_id_] == 0 && value) {
      locks[lock_id_] = aut->GetId();
    } else if (locks[lock_id_] == aut->GetId() && !value) {
      locks[lock_id_] = 0;
    }
  }

 private:
  unsigned lock_id_;
};

bool AutomataRunner::eval_condition(insn_t insn) {
  if ((insn & _IF_STATE_MASK) == _IF_STATE) {
    return (current_automata_->GetState() == (insn & ~_IF_STATE_MASK));
  }
  if ((insn & _IF_REG_MASK) == _IF_REG) {
    int cnt = insn & _IF_REG_BITNUM_MASK;
    uint16_t arg = imported_bit_args_[cnt];
    bool value = GetBit(cnt)->Read(arg, openmrn_node_, current_automata_);
    bool expected = (insn & (1 << 6));
    return value == expected;
  }
  if ((insn & _GET_LOCK_MASK) == _GET_LOCK) {
    int cnt = insn & ~_GET_LOCK_MASK;
    uint16_t arg = imported_bit_args_[cnt];
    ReadWriteBit* bit = GetBit(cnt);
    if (bit->Read(arg, openmrn_node_, current_automata_)) return false;
    bit->Write(arg, openmrn_node_, current_automata_, true);
    return true;
  }
  if ((insn & _REL_LOCK_MASK) == _REL_LOCK) {
    int cnt = insn & ~_REL_LOCK_MASK;
    uint16_t arg = imported_bit_args_[cnt];
    ReadWriteBit* bit = GetBit(cnt);
    bit->Write(arg, openmrn_node_, current_automata_, false);
    return true;
  }
  if ((insn & _IF_MISC_MASK) == _IF_MISC_BASE) {
    switch (insn) {
      case _IF_EMERGENCY_STOP: {
        auto* b =
            openmrn_node_->iface()->global_message_write_flow()->alloc();
        b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT,
                         openmrn_node_->node_id(),
                         openlcb::eventid_to_buffer(
                             openlcb::Defs::EMERGENCY_OFF_EVENT));
        openmrn_node_->iface()->global_message_write_flow()->send(b);
        return true;
      }
      case _IF_EMERGENCY_START: {
        auto* b =
            openmrn_node_->iface()->global_message_write_flow()->alloc();
        b->data()->reset(
            openlcb::Defs::MTI_EVENT_REPORT, openmrn_node_->node_id(),
            openlcb::eventid_to_buffer(
                openlcb::Defs::CLEAR_EMERGENCY_OFF_EVENT));
        openmrn_node_->iface()->global_message_write_flow()->send(b);
        return true;
      }
      case _GET_TRAIN_SPEED: {
        aut_speed_ = get_train_speed();
        return !aut_speed_.isnan();
      }
      case _IF_FORWARD: {
        return (aut_speed_.direction() == openlcb::Velocity::FORWARD);
      }
      case _IF_REVERSE: {
        return (aut_speed_.direction() == openlcb::Velocity::REVERSE);
      }
      case _SET_TRAIN_SPEED: {
        return set_train_speed(aut_speed_);
      }
    }
  }
  diewith(CS_DIE_AUT_HALT);
  return false;
}

bool AutomataRunner::eval_condition2(insn_t insn, insn_t arg2) {
  switch (insn) {
    case _IF_STOP_TRAIN: {
      if (DccLoop_SetLocoPaused(arg2, 1)) return false;
      return true;
    }
    case _IF_START_TRAIN: {
      if (DccLoop_SetLocoPaused(arg2, 0)) return false;
      return true;
    }
    case _IF_STOP_TRAIN_AT: {
      if (DccLoop_SetLocoPaused(train_ids[arg2], 1)) return false;
      return true;
    }
    case _IF_START_TRAIN_AT: {
      if (DccLoop_SetLocoPaused(train_ids[arg2], 0)) return false;
      return true;
    }
    case _IF_MOVE_TRAIN: {
      train_ids[arg2] = train_ids[aut_srcplace_];
      return true;
    }
    case _IF_UNKNOWN_TRAIN_AT: {
      if (DccLoop_IsUnknownLoco(train_ids[arg2]))
        return true;
      else
        return false;
    }
    case _SET_TRAIN_REL_SPEED: {
      if (DccLoop_IsUnknownLoco(train_ids[aut_srcplace_])) {
        return false;
      }
      DccLoop_SetLocoRelativeSpeed(train_ids[aut_srcplace_], arg2);
      return true;
    }
    case _IF_ASPECT: { return (aut_signal_aspect_ == arg2); }
      // EEPROMs should be replaced with standard remote bits.
      /*    case _IF_EEPROM_0:
  case _IF_EEPROM_1: {
      uint8 val, addr;
      addr = arg >> 3;
      val = (EEPROM_READ(addr)) & masks[arg & 7];
      if (insn & 1) {
          return val;
      } else {
          return !val;
      }
      }*/
      /*case _IF_G_0:
  case _IF_G_1: {
      uint8 val, addr;
      addr = arg >> 3;
      val = global_cache[addr] & masks[arg & 7];
      if (insn & 1) {
          return val;
      } else {
          return !val;
      }
      }*/
      /*case _IF_ALIVE: {
      uint8 addr = 0;
      uint8 dd = arg & 0b00111000;
      if (dd == 0b100000 || dd == 0b110000) return 1;
      else if (dd == 0b011000) addr = global1;
      else if (dd == 0b101000) addr = global2;
      else return 0;
      unsigned ret = rpc(addr, CMD_QUERYBITS, QBS_GLOBAL | (QBG_GLOBALBITS <<
      2), 0xa5);
      if (ret & 0xff00) return 0;
      if (ret & masks[arg & 7]) return 0;
      return 1;
      }*/
  }

  diewith(CS_DIE_AUT_HALT);
  return false;
}

void AutomataRunner::eval_action(insn_t insn) {
  if ((insn & _ACT_STATE_MASK) == _ACT_STATE) {
    current_automata_->SetState(insn & ~_ACT_STATE_MASK);
  } else if ((insn & _ACT_TIMER_MASK) == _ACT_TIMER) {
    current_automata_->SetTimer(insn & ~_ACT_TIMER_MASK);
  } else if ((insn & _ACT_REG_MASK) == _ACT_REG) {
    int cnt = insn & _IF_REG_BITNUM_MASK;
    uint16_t arg = imported_bit_args_[cnt];
    GetBit(cnt)->Write(arg, openmrn_node_, current_automata_, insn & (1 << 6));
  } else if ((insn & _ACT_MISC_MASK) == _ACT_MISC_BASE) {
    switch (insn) {
      case _ACT_UP_ASPECT: {
        uint8_t u = aut_signal_aspect_;
        switch (u) {
          case A_DARK:
          case A_STOP:
          case A_SHUNT:
            aut_signal_aspect_ = A_40;
            break;
          case A_40:
          case A_S40:
            aut_signal_aspect_ = A_60;
            break;
          case A_60:
            aut_signal_aspect_ = A_90;
            break;
          case A_90:
            aut_signal_aspect_ = A_GO;
            break;
          case A_GO:
            break;
          default:
            aut_signal_aspect_ = A_DARK;
        }
        break;
      }
      case _ACT_SPEED_FORWARD: {
        aut_speed_.forward();
        break;
      }
      case _ACT_SPEED_REVERSE: {
        aut_speed_.reverse();
        break;
      }
      case _ACT_SPEED_FLIP: {
        aut_speed_.set_direction(1-aut_speed_.direction());
        break;
      }
      default:
        diewith(CS_DIE_AUT_HALT);
    }
  } else {
    diewith(CS_DIE_AUT_HALT);
  }
}

void AutomataRunner::eval_action2(insn_t insn, insn_t arg) {
  switch (insn) {
    case _ACT_SET_TRAINID: {
      aut_trainid_ = arg;
      return;
    }
    case _ACT_SET_SRCPLACE: {
      aut_srcplace_ = arg;
      return;
    }
    case _ACT_SET_SIGNAL: {
      set_signal_aspect(arg, aut_signal_aspect_);
      return;
    }
    case _ACT_SET_ASPECT: {
      aut_signal_aspect_ = arg;
      return;
    }
    case _ACT_READ_GLOBAL_ASPECT: {
      aut_signal_aspect_ = get_signal_aspect(arg);
      return;
    }
    case _ACT_SET_VAR_VALUE_ASPECT: {
      unsigned offset = arg >> 5;
      unsigned var = arg & 31;
      GetBit(var)->SetState(offset, aut_signal_aspect_);
      return;
    }
    case _ACT_GET_VAR_VALUE_ASPECT: {
      unsigned offset = arg >> 5;
      unsigned var = arg & 31;
      aut_signal_aspect_ = GetBit(var)->GetState(offset);
      return;
    }
    case _ACT_GET_VAR_VALUE_SPEED: {
      unsigned offset = arg >> 5;
      unsigned var = arg & 31;
      arg = GetBit(var)->GetState(offset);
    } /* fall through */
    case _ACT_IMM_SPEED: {
      uint8_t value = arg;
      aut_speed_ = 0;
      aut_speed_.set_mph(value & 0x7f);
      if (value & 0x80) {
        aut_speed_.reverse();
      } else {
        aut_speed_.forward();
      }
      return;
    }
    case _ACT_SCALE_SPEED: {
      if (arg & 0x80) {
        aut_speed_.set_direction(1-aut_speed_.direction());
      }
      float scale = arg & 0x7f;
      scale /= 32;
      aut_speed_ *= scale;
      return;
    }
  }
  diewith(CS_DIE_AUT_HALT);
}

bool AutomataRunner::set_train_speed(openlcb::Velocity v) {
  auto* b = openmrn_node_->iface()->addressed_message_write_flow()->alloc();
  b->data()->reset(openlcb::Defs::MTI_TRACTION_CONTROL_COMMAND,
                   openmrn_node_->node_id(),
                   {aut_eventids_[0], 0},
                   openlcb::TractionDefs::speed_set_payload(v));
  openmrn_node_->iface()->addressed_message_write_flow()->send(b);
  return true;
}

openlcb::Velocity AutomataRunner::get_train_speed() {
  HASSERT(traction_.get());
  auto* b = openmrn_node_->iface()->addressed_message_write_flow()->alloc();
  b->data()->reset(openlcb::Defs::MTI_TRACTION_CONTROL_COMMAND,
                   openmrn_node_->node_id(),
                   {aut_eventids_[0], 0},
                   openlcb::TractionDefs::speed_get_payload());
  /** @TODO(balazs.racz) This timeout is way too long -- we want to run
   * automatas at 10 Hz. We really should not block the current thread for so
   * long. Ideally of course this response would arrive much faster, like
   * within a msec if local CS, or a few msecs if remote. */
  traction_->timer_.start(MSEC_TO_NSEC(100));
  traction_->resp_handler_.wait_for_response(b->data()->dst,
                                             b->data()->payload[0],
                                             &traction_->timer_);
  openmrn_node_->iface()->addressed_message_write_flow()->send(b);
  traction_->timer_.wait_for_notification();
  traction_->resp_handler_.wait_timeout();
  if (!traction_->resp_handler_.response()) {
    LOG(VERBOSE,
        "automata: Timeout waiting for traction response from 0x%016" PRIx64
        ".", aut_eventids_[0]);
    return openlcb::nan_to_speed();
  } else {
    openlcb::Velocity r;
    if (!openlcb::TractionDefs::speed_get_parse_last(
             traction_->resp_handler_.response()->data()->payload, &r)) {
      LOG(WARNING, "automata: Invalid traction response from 0x%016" PRIx64 ".",
          aut_eventids_[0]);
      return openlcb::nan_to_speed();
    }
    return r;
  }
}

void AutomataRunner::WaitForWakeup() { os_sem_wait(&automata_sem_); }

void AutomataRunner::TriggerRun() { os_sem_post(&automata_sem_); }

void AutomataRunner::AddPendingTick() {
  if (automata_running()) {
    ++pending_ticks_;
  }
}

void AutomataRunner::RunAllAutomata() {
  if (pending_ticks_) {
    for (auto* aut : all_automata_) {
      aut->Tick();
    }
    --pending_ticks_;
  }
  for (auto* aut : all_automata_) {
    ResetForAutomata(aut).Run();
  }
}

DECLARE_CONST(automata_init_backoff);

void AutomataRunner::InitializeState() {
  while (openmrn_node_ && !openmrn_node_->is_initialized()) {
    usleep(2000);
  }
  // This is only called when running with_thread.
  CreateVarzAndAutomatas();
  for (auto it : declared_bits_) {
    it.second->Initialize(openmrn_node_);
    do {
      usleep(config_automata_init_backoff());
    } while (openlcb::EventService::instance->event_processing_pending());
  }
  pending_ticks_ = 0;
}

class AutomataTick : public Timer {
 public:
  AutomataTick(AutomataRunner* runner)
      : Timer(runner->node()->iface()->executor()->active_timers()),
        runner_(runner), count_(0), exit_(false) {
    start(MSEC_TO_NSEC(100)); // 10 Hz
  }

  long long timeout() OVERRIDE {
    OSMutexLock l(&lock_);
    if (exit_) {
      return Timer::DELETE;
    }
    if (count_++ > 10) {
      runner_->AddPendingTick();
      count_ -= 10;
    }
    runner_->TriggerRun();
    return Timer::RESTART;
  }

  // After this call returns, it is guaranteed that the timer will not call
  // into the runner anymore.
  void request_exit() {
    {
      OSMutexLock l(&lock_);
      exit_ = true;
    }
    runner_->node()->iface()->executor()->sync_run([this]() {
        Timer::trigger();
      });
  }

 private:
  OSMutex lock_;
  AutomataRunner* runner_;
  int count_;
  bool exit_;
};

void* automata_thread(void* arg) {
  AutomataRunner* runner = (AutomataRunner*)arg;
  runner->automata_timer_ = new AutomataTick(runner);
  while (1) {
    AutomataRunner::RunState state;
    Notifiable* n = nullptr;
    {
      OSMutexLock l(&runner->control_lock_);
      state = runner->run_state_;
      if (runner->stop_notification_) {
        std::swap(n, runner->stop_notification_);
      }
    }
    if (state == AutomataRunner::RunState::INIT) {
      runner->InitializeState();
      runner->run_state_ = AutomataRunner::RunState::RUN;
    }
    if (n) {
      n->notify();
    }
    if (state == AutomataRunner::RunState::EXIT) {
      runner->automata_timer_->request_exit();
      return nullptr;
    }
    if (state == AutomataRunner::RunState::RUN) {
      runner->RunAllAutomata();
      UpdateSlaves();
    }
    runner->WaitForWakeup();
  }
  return NULL;
}

AutomataRunner::Traction::Traction(openlcb::Node* node)
    : resp_handler_(node->iface(), node),
      timer_(node->iface()->executor()->active_timers()) {}

AutomataRunner::AutomataRunner(openlcb::Node* node, const insn_t* base_pointer,
                               bool with_thread)
    : ip_(0),
      aut_srcplace_(254),
      aut_trainid_(254),
      aut_signal_aspect_(254),
      base_pointer_(base_pointer),
      current_automata_(NULL),
      openmrn_node_(node),
      traction_(node ? new Traction(node) : nullptr),
      pending_ticks_(0),
      run_state_(RunState::INIT) {
  HASSERT(openmrn_node_);
  automata_write_helper.set_wait_for_local_loopback(true);
  memset(imported_bits_, 0, sizeof(imported_bits_));
  os_sem_init(&automata_sem_, 0);
  if (with_thread) {
    os_thread_create(&automata_thread_handle_, "automata", 1,
                     AUTOMATA_THREAD_STACK_SIZE, automata_thread, this);
  } else {
    CreateVarzAndAutomatas();
    automata_thread_handle_ = 0;
    automata_timer_ = nullptr;
    run_state_ = RunState::NO_THREAD;
  }
}

AutomataRunner::~AutomataRunner() {
  if (0) fprintf(stderr, "Destroying automata runner.\n");
  SyncNotifiable n;
  Stop(&n, true);
  n.wait_for_notification();

  os_sem_destroy(&automata_sem_);
}

void AutomataRunner::Start() {
  OSMutexLock l(&control_lock_);
  run_state_ = RunState::INIT;
}

void AutomataRunner::Stop(Notifiable* n, bool request_exit) {
  Notifiable* nn = nullptr;
  {
    OSMutexLock l(&control_lock_);
    stop_notification_ = new TempNotifiable([this, n]() {
      for (auto i : all_automata_) {
        delete i;
      }
      all_automata_.clear();
      for (auto& i : declared_bits_) {
        delete i.second;
      }
      declared_bits_.clear();
      n->notify();
    });
    if (run_state_ == RunState::NO_THREAD) {
      // maybe we are running without thread?
      std::swap(nn, stop_notification_);
    }
    if (request_exit) {
      run_state_ = RunState::EXIT;
    } else {
      run_state_ = RunState::STOP;
    }
  }
  if (nn) nn->notify();
  TriggerRun();
}
