#include "executor.hxx"

#include "os/OS.hxx"

static void* runnable_thread_body(void* arg) {
  Runnable* r = static_cast<Runnable*>(arg);
  r->Run();
  return NULL;
}

void Runnable::RunInNewThread(const char* name, int priority,
                              size_t stack_size) {
  OSThread thread(name, priority, stack_size, &runnable_thread_body, this);
}
