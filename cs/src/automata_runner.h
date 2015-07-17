#ifndef _BRACZ_TRAIN_AUTOMATA_RUNNER_H
#define _BRACZ_TRAIN_AUTOMATA_RUNNER_H

#include <stdio.h>
#include <string.h>

#include <map>
#include <memory>
#include <vector>
using std::map;
using std::vector;

#include "cs_config.h"
#include "os/os.h"
#include "base.h"
#include "automata_control.h"
#include "automata_defs.h"

#include "nmranet/Velocity.hxx"
#include "nmranet/TractionClient.hxx"
#include "executor/Timer.hxx"
#include "os/OS.hxx"

typedef unsigned aut_offset_t;
typedef uint8_t insn_t;

namespace nmranet { class Node; }

class Automata;

class ReadWriteBit {
public:
  virtual ~ReadWriteBit() {}
  virtual bool Read(uint16_t arg, nmranet::Node* node, Automata* aut) = 0;
  virtual void Write(uint16_t arg, nmranet::Node* node, Automata* aut, bool value) = 0;
  virtual uint8_t GetState(uint16_t arg) { HASSERT(0); return 0; }
  virtual void SetState(uint16_t arg, uint8_t state) { HASSERT(0); }
  virtual void Initialize(nmranet::Node* node) = 0;
};


class Automata {
public:
    Automata(int id, aut_offset_t starting_offset)
	: timer_bit_(id), starting_offset_(starting_offset) {
      if (0) fprintf(stderr,"created automata id %d offset %d\n", id, starting_offset_);
    }

    aut_offset_t GetStartingOffset() {
	return starting_offset_;
    }

    uint8_t GetState() {
	return timer_bit_.state_;
    }

    void SetState(uint8_t state) {
        extern int debug_variables;
        if (debug_variables > 1) {
            LOG(VERBOSE, "Automata %d state to %d", timer_bit_.GetId(), state);
        }
	timer_bit_.state_ = state;
    }

    uint8_t GetTimer() {
	return timer_bit_.timer_;
    }

    void SetTimer(uint8_t value) {
      if (value == (0xff & (~_ACT_TIMER_MASK))) {
	    timer_bit_.timer_ = GetId() >> 3;
      } else {
	timer_bit_.timer_ = value;
      }
    }

    // Decreases any pending timer by one.
    void Tick() {
	if (timer_bit_.timer_) --timer_bit_.timer_;
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
      TimerBit(int id) : id_(id), state_(0), timer_(0) {
          HASSERT(0 <= id && id <= 255);
        }
	virtual ~TimerBit() {}
        virtual bool Read(uint16_t, nmranet::Node*, Automata* aut) {
            return timer_;
	}
	virtual void Write(uint16_t, nmranet::Node*, Automata* aut, bool value) {
	    diewith(CS_DIE_AUT_WRITETIMERBIT);
	}
        void Initialize(nmranet::Node*) OVERRIDE {}
	int GetId() {
	    return id_;
	}
      /*uint8_t* GetStateByte(int offset) {
	    return get_state_byte(id_ >> 3, (id_ & 7) * LEN_AUTOMATA + offset);
            }*/
    private:
        friend class Automata;
        uint8_t id_;
        uint8_t state_;
        uint8_t timer_;
    };

    TimerBit timer_bit_;
    aut_offset_t starting_offset_;
};


class AutomataTick;

class AutomataRunner {
public:
  AutomataRunner(nmranet::Node* node, const insn_t* base_pointer,
                 bool with_thread = true);
    ~AutomataRunner();

  nmranet::Node* node() { return openmrn_node_; }

  // Starts the automata runner from the beginning.
  void Start();
  // Stops and clears all automata runner internal state. if request_exit, also
  // stops the runner thread. Triggers notifiable when done.
    void Stop(Notifiable* n, bool request_exit = false);

    AutomataRunner& ResetForAutomata(Automata* aut);

    //! Simulates the current automata until EOF.
    void Run();

    insn_t get_insn(aut_offset_t offset) {
      insn_t ret = base_pointer_[offset];
      if (0) fprintf(stderr, "get insn[%d] = 0x%02x\n", offset, ret);
      return ret;
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

    //! Do the program-specific part of the initialization.
    void CreateVarzAndAutomatas();

    /// Sends out query messages for every bit.
    void InitializeState();

  //===============Accessors for testing================

  //! Injects a new ReadWriteBit into the global bits that are known by this
  //! runner. Useful for unittests.
  //
  //! @param offset will be the reference for this bit.
  //
  //! @param bit is the new bit to inject; ownership will be transferred to the
  //! runner object.
  void InjectBit(aut_offset_t offset, ReadWriteBit* bit);

  // Testing only - Returns detected list of automatas.
  const vector<Automata*>& GetAllAutomatas() {
    return all_automata_;
  }

  uint8_t GetSrcPlace() { return aut_srcplace_; }
  uint8_t GetTrainId() { return aut_trainid_; }
  uint8_t GetSignalAspect() { return aut_signal_aspect_; }
  uint64_t GetEventId(int idx) { return aut_eventids_[idx]; }

  enum class RunState {
    INIT = 0,
    RUN,
    STOP,
    EXIT,
    NO_THREAD,
  };

private:
    ReadWriteBit* GetBit(int offset) {
	if (!imported_bits_[offset]) {
            LOG_ERROR("Bit %d not imported, ip: %d", offset, ip_);
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
    //! Evaluates an ACT_DEF_VAR. Returns a new variable. Uses load_insn() to
    //! read arguments.
    ReadWriteBit* create_variable();

    //! Changes one of the eventid accumulators.
    void insn_load_event_id();

    /** @returns the last commanded speed of the current loco, or NAN if there
     * was an error getting the speed. */
    nmranet::Velocity get_train_speed();

    /** Sets the speed of the current loco.
     * @returns true on success. */
    bool set_train_speed(nmranet::Velocity speed);

    //! Instruction pointer.
    aut_offset_t ip_;

    uint8_t aut_srcplace_; //< "current" (or source) place of train.
    uint8_t aut_trainid_;  //< train id for absolute identification of trains.
    uint8_t aut_signal_aspect_; //< signal aspect for the next set-signal cmd.
    nmranet::Velocity aut_speed_; //< speed for next set-speed cmd.

    uint64_t aut_eventids_[2];  //< Eventid accumulators for declaring bits.

    //! Points to the beginning of the automata program area.
    const insn_t* base_pointer_;

    vector<Automata*> all_automata_;

    typedef map<aut_offset_t, ReadWriteBit*> DeclaredBitsMap;
    //! Remembers which instruction offsets had bits declared, and the pointers
    //! to those bits. TODO(bracz); who owns these objects?
    DeclaredBitsMap declared_bits_;
    //! The bits that are imported to the current automata. This gets filled up
    //! during the automata code execution.
    ReadWriteBit* imported_bits_[MAX_IMPORT_VAR];
    //! Arguments to the imported bits.
    uint16_t imported_bit_args_[MAX_IMPORT_VAR];

    //! Points to the current automata.
    Automata* current_automata_;
    //! The OpenMRN node used for generating sourced events.
    nmranet::Node* openmrn_node_;
    struct Traction {
        Traction(nmranet::Node* node);
        //! Helper flow for traction requests.
        nmranet::TractionResponseHandler resp_handler_;
        SyncTimeout timer_;
    };
    std::unique_ptr<Traction> traction_;

    //! Counts how many ticks we need to apply in the next run of the automatas.
    int pending_ticks_;

    //! Timer used for repeatedly waking up the automata thread. This pointer
    //! is self-owned, it is not null iff the automata thread is running, and
    //! in that case the thread will ask the timer to delete itself before
    //! exiting.
    AutomataTick* automata_timer_;
    //! Semaphore used for waking up the automata thread.
    os_sem_t automata_sem_;
    //! Mutex to control access to request_thread_exit_, stop_notification_ and is_running_.
    OSMutex control_lock_;
    RunState run_state_;
    //! Notified once when the Stop command completes.
    Notifiable* stop_notification_ = nullptr;
    //! This thread will normally execute the automata code as triggered by
    //! timers.
    os_thread_t automata_thread_handle_;

    friend void* automata_thread(void*);
};


#endif // _BRACZ_TRAIN_AUTOMATA_RUNNER_H
