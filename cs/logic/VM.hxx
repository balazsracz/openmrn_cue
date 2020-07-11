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
#include <limits.h>

#include <string>
#include <functional>
#include <vector>
#include <map>

#include "logic/Variable.hxx"

#include "utils/macros.h"

namespace logic {

/// Virtual machine for executing compiled bytecode.
class VM {
 public:
  /// Constructor.
  /// @param factory is used to instantiate variables duing the execution of
  /// the VM. Ownership is not transferred.
  VM(VariableFactory* factory)
      : variable_factory_(factory),
        block_num_(0),
        is_preamble_(0),
        access_error_(0) {
    factory->set_access_error_callback(std::bind(&VM::access_error, this));
  }

  /// Executes instructions from the current IP. Return true if execution
  /// succeeded (hit the last byte or a TERMINATE command), false if an
  /// exception was generated.
  bool execute();
  
  bool execute(const std::string& ops) {
    /// @todo should be rewritten.
    set_block_code(block_num_, ops);
    return execute_block(block_num_);
  }

  /// Runs a single block of code.
  bool execute_block(uint8_t block_num) {
    set_block_num(block_num);
    jump(get_block_start_ip());
    return execute();
  }

  /// Sets whether the VM should be running in preamble mode or regular mode.
  /// @param is_preamble true if this is the preamble.
  void set_preamble(bool is_preamble) {
    is_preamble_ = is_preamble;
  }

  /// Call this after compile and pass in the compiled bytecode.
  /// The given code can then be executed by execute_block().
  void set_block_code(uint8_t block_num, std::string code) {
    HASSERT(code.size() < (1u << BLOCK_CODE_IP_SHIFT));
    if (blocks_.size() <= block_num) {
      blocks_.resize(block_num + 1);
    }
    blocks_[block_num].code_.swap(code);
  }

  void clear_block_code(uint8_t block_num) {
    if (blocks_.size() > block_num) {
      std::string s;
      blocks_[block_num].code_.swap(s);
    }
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

  /// Defines the value in the "block_num" parameter of variable creation
  /// requests.
  void set_block_num(unsigned block_num) {
    block_num_ = block_num;
  }

  /// Resets the internal state of the virtual machine.
  void clear() {
    operand_stack_.clear();
    variable_stack_.clear();
    call_stack_.clear();
    call_stack_.emplace_back();
    call_stack_.back().fp = 0;
    fp_ = 0;
    access_error_ = 0;
  }

  /// Extracts all variables in GUID range of [begin, end) into a temporary
  /// holding area.
  void save_variables(unsigned begin, unsigned end);
  /// Deletes all variables in the temporary holding area.
  void destroy_saved_variables();
    
  typedef std::map<unsigned, std::unique_ptr<Variable> > ExternalVariableMap;

  typedef unsigned ip_t;

 private:
  friend class BytecodeTest;
  friend class VMTest;
  friend class VMAbsoluteVariable;

  /// Number of bits reserved for IPs within a single block.
  static constexpr unsigned BLOCK_CODE_IP_SHIFT = 16;

  /// @return the IP pointing to the start of the current block.
  ip_t get_block_start_ip();

  /// @return the IP pointing to the start of the current block.
  ip_t get_ip();
  
  /// Set the current execution pointer to the given target. Updates internal
  /// caches.
  /// @return false if the jump target is invalid.
  bool jump(ip_t target);

  /// @return true if the current IP points outside of the valid range.
  inline bool at_eof();

  /// Retrieves the next instruction from the instruction stream. Increments
  /// the instruction pointer.
  /// @return the uint8 of the next instruction value.
  inline uint8_t fetch_insn();
  
  /// Reads a varint from the instruction stream.
  /// @param output the data goes here.
  /// @return true if a varint was successfully read; false if eof was hit.
  bool parse_varint(int* output);

  /// Reads a string from the instruction stream and stores it in the stirng
  /// accumulator.
  /// @return true if a string was successfully read; false if eof was hit.
  bool parse_string();
  
  /// Adds an unexpected EOF error.
  /// @param where will be appended to the error string.
  /// @return false
  bool unexpected_eof(const char* where);

  /// Adds a Variable Access Error.
  void access_error();

  struct ExecutionEnvironment {
    /// Frame pointer. Indexes into the operand_stack_ to define the base for
    /// all relative offset variables. When exiting a function, the operand
    /// stack is truncated to this size.
    unsigned fp;

    /// Frame pointer for variable stack. Contains the size of the variable
    /// stack before entering the current execution frame. When returning from
    /// the execution frame, the variables at and above this index are
    /// destructed.
    unsigned vp;
    
    /// Where to return out of this stack frame.
    ip_t return_address {0};
  };

  /// Instances of this struct will be pushed to the variable stack.
  struct VMVariableReference {
    /// Holds ownership of a variable if it was created locally.
    std::unique_ptr<Variable> owned_var;
    /// Non-owned variable. owned_var is always copied here.
    Variable* var;
    /// Argument to supply to the variable calls.
    unsigned arg;
  };

  /// Implementation of the Variable interface that points to a temporary on
  /// the operand stack. This is used when a local variable (from the operand
  /// stack) is passed as a mutable value to a function.
  class VMAbsoluteVariable : public Variable {
   public:
    VMAbsoluteVariable(VM* parent) : parent_(parent) {}

    /// @return the largest valid state value to write or return from this
    /// variable.
    int max_state() override {
      return INT_MAX;
    }

    /// @param parent is the variable factory that created this variable.
    /// @param arg is the index for vector variables, zero if not used.
    /// @return the current value of this variable.
    int read(const VariableFactory* parent, unsigned arg) override {
      if (parent_->operand_stack_.size() <= arg) {
        parent_->access_error();
        return -1;
      }
      return parent_->operand_stack_[arg];
    }

    /// Write a value to the variable.
    /// @param parent is the variable factory that created this variable.
    /// @param arg is the index for vector variables, zero if not used.
    /// @param value is the new (desired) state of the variable.
    void write(const VariableFactory* parent, unsigned arg,
               int value) override {
      if (parent_->operand_stack_.size() <= arg) {
        parent_->access_error();
        return;
      }
      parent_->operand_stack_[arg] = value;
    }

    VM* parent_;
  } operand_stack_variables_{this};
  
  struct BlockInfo {
    /// The compiled bytecode for the block.
    std::string code_;
  };

  /// All the known / registered blocks.
  std::vector<BlockInfo> blocks_;
  
  /// Stack frames of the nested functions.
  std::vector<ExecutionEnvironment> call_stack_;
  
  /// Stack of values for operands.
  std::vector<int> operand_stack_;

  /// Stack of variable references.
  std::vector<VMVariableReference> variable_stack_;

  /// Accumulator of string arguments;
  std::string string_acc_;

  /// Arguments to the variable creation.
  VariableCreationRequest variable_request_;

  /// This object is used to create variables. Externally owned.
  VariableFactory* variable_factory_;
  
  /// Holds (and owns) all external variables that are defined.
  ExternalVariableMap external_variables_;

  /// Holding area for save_variables.
  std::vector<std::unique_ptr<Variable> > external_variable_holding_;
  
  /// Stores the last VM exception.
  std::string error_;

  /// Output callback.
  std::function<void(std::string)> print_cb_;

  /// Next instruction to execute.
  const uint8_t* _block_start_;
  /// Next instruction to execute.
  const uint8_t* _ip_;
  /// Points to eof, which is the first character after ip_ that is not valid,
  /// aka end pointer (right open range).
  const uint8_t* _eof_;
  /// Frame pointer for operand stack (index in the operand stack where the
  /// current function's stack frame is).
  unsigned fp_;
  /// Frame pointer for variable stack (index in the variable stack where the
  /// current function's stack frame is).  @todo this is probably unused.
  //unsigned vp_;
  /// True if we are running preamble mode.
  /// Number of the logic block. Used in variable creation requests and when execution starts.
  unsigned block_num_ : 8;
  /// Block number where the current IP is. 
  unsigned _ip_block_num_ : 8;
  
  unsigned is_preamble_ : 1;
  /// True if the last variable access encountered an error. Set by
  /// access_error()
  unsigned access_error_ : 1;
};


} // namespace logic

#endif // _LOGIC_VM_HXX_
