#ifndef _BRACZ_TRAIN_EXECUTOR_HXX_
#define _BRACZ_TRAIN_EXECUTOR_HXX_

#include <unistd.h>

class Runnable {
public:
  virtual ~Runnable() {}

  virtual void Run() = 0;

  void RunInNewThread(const char* name, int priority, size_t stack_size);
};


#endif // _BRACZ_TRAIN_EXECUTOR_HXX_
