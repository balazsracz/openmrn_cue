#ifndef _BRACZ_TRAIN_UPDATER_HXX_
#define _BRACZ_TRAIN_UPDATER_HXX_

#include <queue>
#include <list>
#include <initializer_list>


#include "cs_config.h"
#include "macros.h"
#include "executor.hxx"
#include "executor/control_flow.hxx"
#include "updater_base.hxx"

// A queue managing the updates of a single channel. This class will manage
// out-of-order updates and priority between different updates. Currently only
// has a single loop that it goes over and over again.
class UpdateQueue {
public:
  UpdateQueue() {}

  UpdateQueue(std::initializer_list<Updatable *> entries)
      : background_queue_(entries) {}

  // Adds an entry to the updater queue. This entry will be updated over and
  // over again at the normal priority. The ownership of the pointer is not
  // transferred.
  void AddRepeatingEntry(Updatable *entry) { background_queue_.push(entry); }

  Updatable *GetNextEntry() {
    ASSERT(!background_queue_.empty());
    Updatable *entry = background_queue_.front();
    background_queue_.pop();
    background_queue_.push(entry);
    return entry;
  }

private:
  // Queue holding entries of the regular update cycle.
  std::queue<Updatable *, std::list<Updatable *> > background_queue_;

  DISALLOW_COPY_AND_ASSIGN(UpdateQueue);
};

// An updater that runs on an update queue and calls the synchronous updates
// one by one.
class SynchronousUpdater : public Runnable {
public:
  SynchronousUpdater() : exit_state_(INIT) {}

  SynchronousUpdater(std::initializer_list<Updatable *> entries)
      : queue_(entries), exit_state_(INIT) {}

  virtual ~SynchronousUpdater() { ASSERT(exit_state_ == EXITED); }

  UpdateQueue *queue() { return &queue_; }

  virtual void Run() {
    exit_state_ = RUN;
    while (exit_state_ == RUN) {
      Step();
    }
    exit_state_ = EXITED;
  }

  void Step() {
    Updatable *next = queue_.GetNextEntry();
    ASSERT(next);
    next->PerformUpdate();
  }

  void RequestExit() {
    if (exit_state_ == RUN) {
      exit_state_ = REQUEST_EXIT;
    }
  }

  bool exited() { return exit_state_ == EXITED; }

private:
  UpdateQueue queue_;

  typedef enum {
    INIT = 0,
    RUN,
    REQUEST_EXIT,
    EXITED
  } ExitState;

  ExitState exit_state_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousUpdater);
};

class FlowUpdater : public ControlFlow {
public:
  FlowUpdater(Executor *executor, std::initializer_list<Updatable *> entries)
      : ControlFlow(executor, EmptyNotifiable::DefaultInstance()),
        queue_(entries) {
    StartFlowAt(ST(StartUpdateTimer));
  }

  UpdateQueue* queue() { return &queue_; }

  ControlFlowAction StartUpdateTimer() {
    //WakeUpRepeatedly(&sleep_data_, MSEC_TO_PERIOD(50));
    return CallImmediately(ST(WaitForWakeup));
  }

  ControlFlowAction WaitForWakeup() {
    return Sleep(&sleep_data_, MSEC_TO_NSEC(5), ST(UpdateOne));
    //return WaitForTimerWakeUpAndCall(&sleep_data_, ST(UpdateOne));
  }

  ControlFlowAction UpdateOne() {
    Updatable *next = queue_.GetNextEntry();
    ASSERT(next);
    next->PerformUpdate();
    return YieldAndCall(ST(WaitForWakeup));
  }

private:
  UpdateQueue queue_;
  SleepData sleep_data_;
};

#endif
