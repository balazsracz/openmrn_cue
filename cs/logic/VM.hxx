/** \copyright
 * Copyright (c) 2019, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file VM.hxx
 *
 * Virtual Machine to execute the logic language's compiled bytecode.
 *
 * @author Balazs Racz
 * @date 15 June 2019
 */

#ifndef _LOGIC_VM_HXX_
#define _LOGIC_VM_HXX_

#include <stdint.h>
#include <unistd.h>

#include <string>
#include <functional>
#include <vector>
#include <map>

#include "logic/Variable.hxx"

namespace logic {

/// Virtual machine for executing compiled bytecode.
class VM {
 public:
  /// Constructor.
  /// @param factory is used to instantiate variables duing the execution of
  /// the VM. Ownership is not transferred.
  VM(const VariableFactory* factory) : variable_factory_(factory) {}

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
    fp_ = 0;
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

  /// This object is used to create variables. Externally owned.
  const VariableFactory* variable_factory_;
  
  typedef std::map<unsigned, std::unique_ptr<Variable> > ExternalVariableMap;

  /// Holds (and owns) all external variables that are defined.
  ExternalVariableMap external_variables_;

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
  /// Frame pointer (index in the operand stack where the current function's
  /// stack frame is).
  unsigned fp_;
};


} // namespace logic

#endif // _LOGIC_VM_HXX_
