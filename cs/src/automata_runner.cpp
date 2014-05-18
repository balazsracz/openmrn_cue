#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE
#endif
#ifndef __STDC_VERSION__
#define __STDC_VERSION__ 199901L
#endif

//#define LOGLEVEL VERBOSE
// Workaroundfor bug in <memory> for gcc 2.6.2 lpcxpresso newlib
#ifndef __CR2_C___4_6_2_BITS_SHARED_PTR_H__
#define __CR2_C___4_6_2_BITS_SHARED_PTR_H__
#endif

#include <stdio.h>

#include <memory>

#include "utils/macros.h"
//#include "core/nmranet_event.h"

#include "common_event_handlers.hxx"

#include "automata_defs.h"
#include "automata_runner.h"
#include "automata_control.h"
#include "dcc-master.h"

#include "utils/logging.h"
#include "nmranet/NMRAnetWriteFlow.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/GlobalEventHandler.hxx"

// This write helper will only ever be used synchronously.
static NMRAnet::WriteHelper automata_write_helper;
static SyncNotifiable g_notify_wait;
static BarrierNotifiable g_barrier_notify;

static BarrierNotifiable* get_notifiable() {
  return g_barrier_notify.reset(&g_notify_wait);
}

static void wait_for_notification() {
  g_notify_wait.wait_for_notification();
}

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
    do {
        int ofs = load_insn();
        ofs |= load_insn() << 8;
        LOG(VERBOSE, "read automata ofs: ofs %d\n", ofs);
        if (!ofs) break;
        all_automata_.push_back(new ::Automata(id++, ofs));
    } while(1);
    // This will execute all preamble commands, including the variable create
    // commands.
    Run();
}

void AutomataRunner::Run() {
    uint8_t insn;
    uint8_t numif, numact;
    int endif, endcond;
    bool keep;
    while (1) {
      //LOG(VERBOSE, "ip: %d\n", ip_);
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
    }
}

void AutomataRunner::import_variable() {
    uint8_t local_idx = load_insn();
    uint16_t arg = local_idx >> 5;
    local_idx &= 31;
    // Need to rewrite the bit fields if we want a different local variable set
    // size.
    HASSERT(32 == (sizeof(imported_bits_) /
                   sizeof(imported_bits_[0])));
    arg <<= 8;
    arg |= load_insn();
    uint16_t global_ofs = load_insn();
    global_ofs |= load_insn() << 8;
    if (local_idx >= (sizeof(imported_bits_) /
                      sizeof(imported_bits_[0]))) {
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
    EventBit(NMRAnet::AsyncNode* node,
             uint64_t event_on, uint64_t event_off,
             uint8_t mask, uint8_t* ptr)
        : bit_(node, event_on, event_off, ptr, mask),
          pc_(&bit_),
          defined_(false) {
        if (0) fprintf(stderr,"event bit create on node %p\n", node);
    }

    virtual void Initialize(NMRAnet::AsyncNode*) {
      pc_.SendQuery(&automata_write_helper, get_notifiable());
      wait_for_notification();
    }

    virtual bool Read(uint16_t, NMRAnet::AsyncNode*, Automata* aut) {
      // TODO(balazs.racz): we should consider CHECK failing here if
      // !defined. That will force us to explicitly reset every bit in StInit.
      return bit_.GetCurrentState();
    }

    virtual void Write(uint16_t, NMRAnet::AsyncNode* node, Automata* aut, bool value) {
        if (0) fprintf(stderr,"event bit write to node %p\n", node);
        bool last_value = bit_.GetCurrentState();
        if ((value == last_value) && defined_) return;
        bit_.SetState(value);
        pc_.SendEventReport(&automata_write_helper, get_notifiable());
        wait_for_notification();
        defined_ = true;
    }

 private:
  NMRAnet::MemoryBit<uint8_t> bit_;
  NMRAnet::BitEventPC pc_;
  // This bit is true if we've already seen an event that defines this bit.
  bool defined_;
};

class EventBlockBit : public ReadWriteBit {
 public:
  EventBlockBit(NMRAnet::WriteHelper::node_type node,
                uint64_t event_base,
                size_t size)
      : storage_(new uint32_t[(size + 31) >> 5]),
        handler_(new NMRAnet::BitRangeEventPC(node, event_base, storage_, size)) {
    size_t sz = (size + 31) >> 5;
    memset(&storage_[0], 0, sz * sizeof(storage_[0]));
  }

  ~EventBlockBit() {
    delete[] storage_;
  }

  virtual bool Read(uint16_t arg, NMRAnet::AsyncNode*, Automata* aut) {
    return handler_->Get(arg);
  }

  virtual void Write(uint16_t arg, NMRAnet::AsyncNode*, Automata* aut, bool value) {
    handler_->Set(arg, value, &automata_write_helper, get_notifiable());
    wait_for_notification();
  }

 private:
  uint32_t* storage_;
  std::unique_ptr<NMRAnet::BitRangeEventPC> handler_;
};

class EventByteBlock : public ReadWriteBit {
 public:
  EventByteBlock(NMRAnet::WriteHelper::node_type node,
                 uint64_t event_base,
                 size_t size)
      : storage_(new uint8_t[size]),
        handler_(new NMRAnet::ByteRangeEventP(node, event_base, storage_, size)) {
      memset(&storage_[0], 0, size);
  }

  ~EventByteBlock() {
    delete[] storage_;
  }

  virtual bool Read(uint16_t arg, NMRAnet::AsyncNode*, Automata* aut) {
      HASSERT(0);
  }

  virtual void Write(uint16_t arg, NMRAnet::AsyncNode*, Automata* aut, bool value) {
      HASSERT(0);
  }

  virtual uint8_t GetState(uint16_t arg) {
      return storage_[arg];
  }

  virtual void SetState(uint16_t arg, uint8_t state) {
      if (storage_[arg] != state) {
          storage_[arg] = state;
          handler_->Update(arg, &automata_write_helper, get_notifiable());
          wait_for_notification();
      }
  }

 private:
  uint8_t* storage_;
  std::unique_ptr<NMRAnet::ByteRangeEventP> handler_;
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
                            (1<<bit), ptr);
    case 1: {
        uint16_t size = ((client & 7) << 8) | arg2;
        return new EventBlockBit(openmrn_node_, aut_eventids_[0], size);
    }
    case 2: {
        return new EventByteBlock(openmrn_node_, aut_eventids_[0], arg2);
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
    while(1) {
        aut_eventids_[dest] &= ~(0xffULL<<ofs);
        aut_eventids_[dest] |= (uint64_t(load_insn()) << ofs);
        if (!ofs) break;
        ofs -= 8;
    }
}



class LockBit : public ReadWriteBit {
public:
    LockBit(unsigned offset) : lock_id_(offset) {
	ASSERT(offset < MAX_LOCK_ID);
    }
    virtual ~LockBit();

    virtual bool Read(uint16_t, node_t node, Automata* aut) {
	int existing_id = locks[lock_id_];
	if (!existing_id) return false;
	if (existing_id == aut->GetId()) return false;
	return true;
    }

    virtual void Write(uint16_t, node_t node, Automata* aut, bool value) {
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
      bool value = GetBit(cnt)
	  ->Read(arg, openmrn_node_, current_automata_);
      bool expected = (insn & (1<<6));
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
	  DccLoop_EmergencyStop();
	  return true;
      }
      case _IF_EMERGENCY_START: {
	  diewith(CS_DIE_UNSUPPORTED);
	  return false;
      }
      case _IF_CLEAR_TRAIN: {
	  train_ids[aut_srcplace_] = 0;
	  return true;
      }
      case _IF_TRAIN_IS_FORWARD: {
          if (DccLoop_IsUnknownLoco(train_ids[aut_srcplace_]))
	      return false;
	  if (DccLoop_IsLocoReversed(train_ids[aut_srcplace_])) {
            return false;
          }
	  return true;
      }
      case _IF_TRAIN_IS_REVERSE: {
          if (DccLoop_IsUnknownLoco(train_ids[aut_srcplace_]))
	      return false;
	  if (!DccLoop_IsLocoReversed(train_ids[aut_srcplace_])) {
            return false;
          }
	  return true;
      }
      case _SET_TRAIN_FORWARD: {
          if (DccLoop_IsUnknownLoco(train_ids[aut_srcplace_]))
	      return false;
          DccLoop_SetLocoReversed(train_ids[aut_srcplace_], 0);
          return true;
      }
      case _SET_TRAIN_REVERSE: {
          if (DccLoop_IsUnknownLoco(train_ids[aut_srcplace_]))
	      return false;
          DccLoop_SetLocoReversed(train_ids[aut_srcplace_], 1);
          return true;
      }
      case _IF_TRAIN_IS_PUSHPULL: {
          if (DccLoop_IsUnknownLoco(train_ids[aut_srcplace_]))
            return false;
          if (DccLoop_IsLocoPushPull(train_ids[aut_srcplace_])) {
            return true;
          }
          return false;
      }
      case _IF_TRAIN_IS_NOT_PUSHPULL: {
          if (DccLoop_IsUnknownLoco(train_ids[aut_srcplace_]))
            return false;
          if (!DccLoop_IsLocoPushPull(train_ids[aut_srcplace_])) {
            return true;
          }
          return false;
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
	if (DccLoop_SetLocoPaused(train_ids[arg2], 1))
            return false;
	return true;
    }
    case _IF_START_TRAIN_AT: {
	if (DccLoop_SetLocoPaused(train_ids[arg2], 0))
            return false;
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
    case _IF_ASPECT: {
	return (aut_signal_aspect_ == arg2);
    }
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
	unsigned ret = rpc(addr, CMD_QUERYBITS, QBS_GLOBAL | (QBG_GLOBALBITS << 2), 0xa5);
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
	GetBit(cnt)
	    ->Write(arg, openmrn_node_, current_automata_, insn & (1<<6));
    } else if ((insn & _ACT_MISC_MASK) == _ACT_MISC_BASE) {
	switch (insn) {
	case _ACT_UP_ASPECT: {
	    uint8_t u = aut_signal_aspect_;
	    switch (u) {
	    case A_DARK:
	    case A_STOP:
	    case A_SHUNT: aut_signal_aspect_ = A_40; break;
	    case A_40:
	    case A_S40: aut_signal_aspect_ = A_60; break;
	    case A_60: aut_signal_aspect_ = A_90; break;
	    case A_90: aut_signal_aspect_ = A_GO; break;
	    case A_GO: break;
	    default: aut_signal_aspect_ = A_DARK;
	    }
	    break;
	}
	    //default: HALT;
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
    }
    diewith(CS_DIE_AUT_HALT);
}

void AutomataRunner::WaitForWakeup() {
    os_sem_wait(&automata_sem_);
}

void AutomataRunner::TriggerRun() {
    os_sem_post(&automata_sem_);
}

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
    } while (NMRAnet::GlobalEventService::instance->event_processing_pending());
  }
}

static long long automata_tick_callback(void* runner, void*) {
    ASSERT(runner);
    static int count = 0;
    if (count++ > 10) {
	((AutomataRunner*)runner)->AddPendingTick();
	count -= 10;
    }
    ((AutomataRunner*)runner)->TriggerRun();
    return OS_TIMER_RESTART; //SEC_TO_NSEC(1);
}

void* automata_thread(void* arg) {
    AutomataRunner* runner = (AutomataRunner*)arg;
    runner->InitializeState();
    runner->automata_timer_ = os_timer_create(&automata_tick_callback, runner, NULL);
    os_timer_start(runner->automata_timer_, MSEC_TO_NSEC(100));  // 10 HZ
    while(1) {
	runner->WaitForWakeup();
        if (runner->request_thread_exit_) {
          runner->thread_exited_ = true;
          return NULL;
        }
	if (automata_running()) {
	    runner->RunAllAutomata();
            UpdateSlaves();
	}
    }
    return NULL;
}

AutomataRunner::AutomataRunner(NMRAnet::AsyncNode* node, const insn_t* base_pointer,
                               bool with_thread)
    : ip_(0),
      aut_srcplace_(254),
      aut_trainid_(254),
      aut_signal_aspect_(254),
      base_pointer_(base_pointer),
      current_automata_(NULL),
      openmrn_node_(node),
      pending_ticks_(0),
      request_thread_exit_(false),
      thread_exited_(false) {
    automata_write_helper.set_wait_for_local_loopback(true);
    memset(imported_bits_, 0, sizeof(imported_bits_));
    os_sem_init(&automata_sem_, 0);
    if (with_thread) {
      os_thread_create(&automata_thread_handle_, "automata", 1,
                       AUTOMATA_THREAD_STACK_SIZE, automata_thread, this);
    } else {
      CreateVarzAndAutomatas();
      automata_thread_handle_ = 0;
      automata_timer_ = 0;
      thread_exited_ = true;
    }
}

AutomataRunner::~AutomataRunner() {
  if (0) fprintf(stderr,"Destroying automata runner.\n");
  request_thread_exit_ = true;
  TriggerRun();
  // NOTE(balazs.racz) This should call os_thread_join except that API does not
  // exist.
  while (!thread_exited_) {
    // Do nothing.
    usleep(100);
  }
  if (automata_timer_) {
    os_timer_delete(automata_timer_);
  }
  for (auto i : all_automata_) {
    delete i;
  }
  for (auto& i : declared_bits_) {
    delete i.second;
  }
  os_sem_destroy(&automata_sem_);
}
