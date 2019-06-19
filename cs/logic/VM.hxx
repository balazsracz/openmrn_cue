#ifndef _LOGIC_VM_HXX_
#define _LOGIC_VM_HXX_

#include <stdint.h>
#include <unistd.h>

#include <string>
#include <functional>
#include <vector>

namespace logic {

/// Virtual machine for executing compiled bytecode.
class VM {
 public:
  VM() {}

  /// Executes a given set of instructions. Return true if execution succeeded
  /// (hit the last byte or a TERMINATE command), false if an exception was
  /// generated.
  /// @param data is the first instruction to execute;
  /// @param len is the number of bytes to execute.
  bool execute(const void* data, size_t len);
  bool execute(const std::string& ops) {
    return execute(ops.data(), ops.size());
  }

  /// @return exception description if the execution failed.
  const std::string& get_error() {
    return error_;
  }

  /// Sets the virtual machine's output function.
  /// @param cb will be called when the program outputs text.
  void set_output(std::function<void(std::string)> cb) {
    print_cb_ = std::move(cb);
  }

  /// Resets the internal state of the virtual machine.
  void clear() {
    operand_stack_.clear();
    call_stack_.clear();
    call_stack_.emplace_back();
    call_stack_.back().fp = 0;
  }

 private:
  friend class VarintTest;
  friend class VMTest;

  struct ExecutionEnvironment {
    /// Frame pointer. Indexes into the operand_stack_ to define the base for
    /// all relative offset variables. When exiting a function, the operand
    /// stack is truncated to this size.
    unsigned fp;

    /// Where to return out of this stack frame.
    const uint8_t* return_address {nullptr};
  };

  /// Stack frames of the nested functions.
  std::vector<ExecutionEnvironment> call_stack_;
  
  /// Stack of values for operands.
  std::vector<int> operand_stack_;

  /// Reads a varint from the instruction stream.
  /// @param output the data goes here.
  /// @return true if a varint was successfully read; false if eof was hit.
  bool parse_varint(int* output);

  /// Adds an unexpected EOF error.
  /// @param where will be appended to the error string.
  /// @return false
  bool unexpected_eof(const char* where);

  /// Stores the last VM exception.
  std::string error_;

  /// Output callback.
  std::function<void(std::string)> print_cb_;

  /// Next instruction to execute.
  const uint8_t* ip_;
  /// Points to eof, which is the first character after ip_ that is not valid,
  /// aka end pointer (right open range).
  const uint8_t* eof_;
};


} // namespace logic

#endif // _LOGIC_VM_HXX_
