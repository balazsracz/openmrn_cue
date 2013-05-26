#ifndef _BRACZ_TRAIN_AUTOMATA_RUNNER_H
#define _BRACZ_TRAIN_AUTOMATA_RUNNER_H

#include <string.h>

#include <map>
#include <ext/slist>
using std::map;
using __gnu_cxx::slist;

#include "cs_config.h"
#include "os/os.h"
#include "core/nmranet_node.h"
#include "base.h"
#include "automata_control.h"
#include "automata_defs.h"

typedef uint16_t aut_offset_t;
typedef uint8_t insn_t;

class Automata;

class ReadWriteBit {
public:
    virtual ~ReadWriteBit() {}
    virtual bool Read(node_t node, Automata* aut) = 0;
    virtual void Write(node_t node, Automata* aut, bool value) = 0;
};


class Automata {
public:
    Automata(int id, aut_offset_t starting_offset)
	: timer_bit_(id), starting_offset_(starting_offset_) {}

    aut_offset_t GetStartingOffset() {
	return starting_offset_;
    }

    uint8_t GetState() {
	return *timer_bit_.GetStateByte(OFS_STATE);
    }

    void SetState(uint8_t state) {
	*timer_bit_.GetStateByte(OFS_STATE) = state;
    }

    uint8_t GetTimer() {
	return *timer_bit_.GetStateByte(OFS_TIMER);
    }

    void SetTimer(uint8_t state) {
	if (state == (0xff & ~_ACT_TIMER_MASK)) {
	    *timer_bit_.GetStateByte(OFS_TIMER) = GetId() >> 3;
	}
	*timer_bit_.GetStateByte(OFS_TIMER) = state;
    }
    
    // Decreases any pending timer by one.
    void Tick() {
	uint8_t* timer = timer_bit_.GetStateByte(OFS_TIMER);
	if (*timer) --*timer;
    }

    int GetId() {
	return timer_bit_.GetId();
    }

    ReadWriteBit* GetTimerBit() {
	return &timer_bit_;
    }

private:
    class TimerBit : public ReadWriteBit {
    public:
	TimerBit(int id) : id_(id) {}
	virtual ~TimerBit() {}
	virtual bool Read(node_t node, Automata* aut) {
	    return *GetStateByte(OFS_TIMER);
	}
	virtual void Write(node_t node, Automata* aut, bool value) {
	    diewith(CS_DIE_AUT_WRITETIMERBIT);
	}
	int GetId() {
	    return id_;
	}
	uint8_t* GetStateByte(int offset) {
	    return get_state_byte(id_ >> 3, (id_ & 7) * LEN_AUTOMATA + offset);
	}
    private:
	int id_;
    };

    TimerBit timer_bit_;
    aut_offset_t starting_offset_;
};


class AutomataRunner {
public:
    AutomataRunner(node_t node);
    ~AutomataRunner(node_t node);

    AutomataRunner& ResetForAutomata(Automata* aut);

    //! Simulates the current automata until EOF.
    void Run();

    insn_t get_insn(aut_offset_t offset) {
	return base_pointer_[offset];
    }

    insn_t load_insn() {
	return get_insn(ip_++);
    }

    //! Iterates through all automata and runs them once.
    void RunAllAutomata();

    //! Blocks the current thread until someone else calls TriggerRun().
    void WaitForWakeup();
    //! Wakes up the automata thread for processing.
    void TriggerRun();
    //! Tells the next automata run to step the automata counters.
    void AddPendingTick();

private:
    ReadWriteBit* GetBit(int offset) {
	if (!imported_bits_[offset]) {
	    diewith(CS_DIE_AUT_IMPORTERROR);
	}
	return imported_bits_[offset];
    }

    bool eval_condition(insn_t insn);
    bool eval_condition2(insn_t insn, insn_t arg);
    void eval_action(insn_t insn);
    void eval_action2(insn_t insn, insn_t arg);

    // These funcitons will advance the IP as needed to read additional
    // arguments.
    //! Evaluates an ACT_IMPORT_VAR
    void import_variable();
    //! Evaluates an ACT_DEF_VAR. Returns a new variable. May use len bytes of
    //! next instructions.
    ReadWriteBit* create_variable(int len);

    //! Instruction pointer.
    aut_offset_t ip_;

    uint8_t aut_srcplace_; //< "current" (or source) place of train.
    uint8_t aut_trainid_;  //< train id for absolute identification of trains.
    uint8_t aut_signal_aspect_; //< signal aspect for the next set-signal cmd.

    //! Points to the beginning of the automata program area.
    insn_t* base_pointer_;

    slist<Automata*> all_automata_;

    typedef map<aut_offset_t, ReadWriteBit*> DeclaredBitsMap;
    //! Remembers which instruction offsets had bits declared, and the pointers
    //! to those bits. TODO(bracz); who owns these objects?
    DeclaredBitsMap declared_bits_;
    //! The bits that are imported to the current automata. This gets filled up
    //! during the automata code execution.
    ReadWriteBit* imported_bits_[32];
    //! Points to the current automata.
    Automata* current_automata_;
    //! The OpenMRN node used for generating sourced events.
    node_t openmrn_node_;

    //! Counts how many ticks we need to apply in the next run of the automatas.
    int pending_ticks_;

    //! Timer used for repeatedly waking up the automata thread.
    os_timer_t automata_timer_;
    //! Semaphore used for waking up the automata thread.
    os_sem_t automata_sem_;

    friend void* automata_thread(void*);
};


#endif // _BRACZ_TRAIN_AUTOMATA_RUNNER_H
