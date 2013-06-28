#include <stdio.h>

#include "core/nmranet_event.h"

#include "common_event_handlers.hxx"

#include "automata_defs.h"
#include "automata_runner.h"
#include "automata_control.h"
#include "dcc-master.h"


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

AutomataRunner& AutomataRunner::ResetForAutomata(Automata* aut) {
    ASSERT(aut);
    current_automata_ = aut;
    ip_ = aut->GetStartingOffset();
    memset(imported_bits_, 0, sizeof(imported_bits_));
    imported_bits_[0] = aut->GetTimerBit();
    return *this;
}

void AutomataRunner::CreateVarzAndAutomatas() {
    ip_ = 0;
    int id = 0;
    do {
        int ofs = load_insn();
        ofs |= load_insn() << 8;
        if (0) fprintf(stderr, "read automata ofs: ofs %d\n", ofs);
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
    while((insn = load_insn()) != 0) {
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
}


class EventBit : public ReadWriteBit, MemoryToggleEventHandler<uint8_t> {
public:
    EventBit(node_t node,
             uint64_t event_on, uint64_t event_off,
             uint8_t mask, uint8_t* ptr)
        : MemoryToggleEventHandler<uint8_t>(event_on, event_off, mask, ptr) {
        nmranet_event_producer(node, event_on, EVENT_STATE_INVALID);
        nmranet_event_producer(node, event_off, EVENT_STATE_INVALID);
        nmranet_event_consumer(node, event_on, EVENT_STATE_INVALID);
        nmranet_event_consumer(node, event_off, EVENT_STATE_INVALID);
    }

    virtual bool Read(node_t node, Automata* aut) {
        return ((*memory_) & mask_);
    }

    virtual void Write(node_t node, Automata* aut, bool value) {
        if (value) {
            *memory_ |= mask_;
            nmranet_event_produce(node, event_on_, EVENT_STATE_VALID);
            nmranet_event_produce(node, event_off_, EVENT_STATE_INVALID);
        } else {
            *memory_ &= ~mask_;
            nmranet_event_produce(node, event_on_, EVENT_STATE_INVALID);
            nmranet_event_produce(node, event_off_, EVENT_STATE_VALID);
        }
    }
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

    virtual bool Read(node_t node, Automata* aut) {
	int existing_id = locks[lock_id_];
	if (!existing_id) return false;
	if (existing_id == aut->GetId()) return false;
	return true;
    }

    virtual void Write(node_t node, Automata* aut, bool value) {
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
      bool value = GetBit(insn & _IF_REG_BITNUM_MASK)
	  ->Read(openmrn_node_, current_automata_);
      bool expected = (insn & (1<<6));
      return value == expected;
  }
  if ((insn & _GET_LOCK_MASK) == _GET_LOCK) {
      ReadWriteBit* bit = GetBit(insn & ~_GET_LOCK_MASK);
      if (bit->Read(openmrn_node_, current_automata_)) return false;
      bit->Write(openmrn_node_, current_automata_, true);
      return true;
  }
  if ((insn & _REL_LOCK_MASK) == _REL_LOCK) {
      ReadWriteBit* bit = GetBit(insn & ~_REL_LOCK_MASK);
      bit->Write(openmrn_node_, current_automata_, false);
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
	GetBit(insn & _IF_REG_BITNUM_MASK)
	    ->Write(openmrn_node_, current_automata_, insn & (1<<6));
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
    for (auto aut : all_automata_) {
	ResetForAutomata(aut).Run();
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
    runner->automata_timer_ = os_timer_create(&automata_tick_callback, runner, NULL);
    os_timer_start(runner->automata_timer_, MSEC_TO_NSEC(100));  // 10 HZ
    while(1) {
	runner->WaitForWakeup();
        if (runner->request_thread_exit_) {
          runner->thread_exited_ = true;
          return NULL;
        }
	if (runner->pending_ticks_) {
	    for (auto aut : runner->all_automata_) {
		aut->Tick();
	    }
	    --runner->pending_ticks_;
	}
	if (automata_running()) {
	    runner->RunAllAutomata();
	}
    }
    return NULL;
}

AutomataRunner::AutomataRunner(node_t node, insn_t* base_pointer)
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
    memset(imported_bits_, 0, sizeof(imported_bits_));
    CreateVarzAndAutomatas();
    os_sem_init(&automata_sem_, 0);
    os_thread_create(&automata_thread_handle_, "automata", 0,
                     AUTOMATA_THREAD_STACK_SIZE, automata_thread, this);
}

AutomataRunner::~AutomataRunner() {
  if (0) fprintf(stderr,"Destroying automata runner.\n");
  request_thread_exit_ = true;
  TriggerRun();
  // NOTE(balazs.racz) This should call os_thread_join except that API does not
  // exist.
  while (!thread_exited_) {
    // Do nothing.
  }
  os_timer_delete(automata_timer_);
  for (auto i : all_automata_) {
    delete i;
  }
  for (auto& i : declared_bits_) {
    delete i.second;
  }
  os_sem_destroy(&automata_sem_);
}
