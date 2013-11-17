#ifndef  _BRACZ_TRAIN_TIMER_UPDATER_HXX_
#define  _BRACZ_TRAIN_TIMER_UPDATER_HXX_

#include <deque>
#include <initializer_list>

#include "cs_config.h"
#include "macros.h"
#include "os/OS.hxx"

/**
   A class for repeatedly calling an updatable. The updatable should have
   non-blocking code (e.g. a GPIO read is OK, an I2C transaction is not).
 */
class TimerUpdater {
 public:
  TimerUpdater(long long period_nano,
               std::initializer_list<Updatable*> entries)
      : background_queue_(entries),
        timer_(TimerUpdater::callback, this, NULL) {
    timer_.start(period_nano);
  }

  static long long callback(void* object, void* /*unused*/) {
    TimerUpdater* me = static_cast<TimerUpdater*>(object);
    for (auto u : me->background_queue_) {
      u->PerformUpdate();
    }
    return OS_TIMER_RESTART;
  }

 private:
  std::deque<Updatable*> background_queue_;
  long long period_nano_;
  OSTimer timer_;
};


#endif // _BRACZ_TRAIN_TIMER_UPDATER_HXX_
