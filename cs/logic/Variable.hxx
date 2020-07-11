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
 * \file Variable.hxx
 *
 * Interface for variable factories to the VM.
 *
 * @author Balazs Racz
 * @date 25 June 2019
 */

#ifndef _LOGIC_VARIABLE_HXX_
#define _LOGIC_VARIABLE_HXX_

#include <string>
#include <memory>
#include <functional>
#include <limits.h>

namespace logic {

/// This structure is used to parametrize the variable creation interface.
struct VariableCreationRequest {
  VariableCreationRequest() {
    clear();
  }
  /// Resets the request to default state.
  void clear() {
    name.clear();
    default_event = 0;
    default_on_event = 0;
    block_num = 0;
    type = TYPE_BOOL;
    num_states = 2;
  }
  /// Number of logic block in which this variable shall be created.
  uint8_t block_num;

  enum Type : uint8_t {
    TYPE_BOOL,
    TYPE_INT
  };

  /// Variable type.
  Type type{TYPE_BOOL};

  /// Number of states (for int variables).
  int num_states{2};
  
  /// User visible name of the variable (may be syntactical name or a string
  /// provided by the code author).
  std::string name;
  /// If a variable is newly created, and this value is not zero, then it will
  /// be initialized to this event ID. This is the base event ID (for
  /// multiplicity) or the OFF state for binary.
  uint64_t default_event;
  /// If a boolean variable is newly created, and this value is not zero, then
  /// the new variable will be initialized to this as being active / on event
  /// ID.
  uint64_t default_on_event;
};

class VariableFactory;

class Variable {
 public:
  virtual ~Variable() {}

  /// @return the largest valid state value to write or return from this
  /// variable.
  virtual int max_state() = 0;

  /// @param parent is the variable factory that created this variable.
  /// @param arg is the index for vector variables, zero if not used.
  /// @return the current value of this variable.
  virtual int read(const VariableFactory* parent, unsigned arg) = 0;

  /// Write a value to the variable.
  /// @param parent is the variable factory that created this variable.
  /// @param arg is the index for vector variables, zero if not used.
  /// @param value is the new (desired) state of the variable.
  virtual void write(const VariableFactory* parent, unsigned arg,
                     int value) = 0;
};

/// Implementation of the Variable interface that is static, i.e. the value
/// is kept between different runs. The value is stored in the object
/// directly.
class StaticVariable : public Variable {
 public:
  StaticVariable() {}

  /// @return the largest valid state value to write or return from this
  /// variable.
  int max_state() override { return INT_MAX; }

  /// @param parent is the variable factory that created this variable.
  /// @param arg is the index for vector variables, zero if not used.
  /// @return the current value of this variable.
  int read(const VariableFactory* parent, unsigned arg) override {
    return value_;
  }

  /// Write a value to the variable.
  /// @param parent is the variable factory that created this variable.
  /// @param arg is the index for vector variables, zero if not used.
  /// @param value is the new (desired) state of the variable.
  void write(const VariableFactory* parent, unsigned arg, int value) override {
    value_ = value;
  }

  int value_{0};
};

/// Abstract interface to the component that is responsible for creating the
/// external variables.
class VariableFactory {
 protected:
  /// Destructor. Protected as clients will use 
  virtual ~VariableFactory() {}

 public:
  /// Creates a new variable.
  /// @param request parametrizes what kind of variable to create. The
  /// ownership is NOT transferred, but the implementation may destroy/move
  /// away the values in the structure.
  /// @return newly created variable.
  virtual std::unique_ptr<Variable> create_variable(
      VariableCreationRequest* request) = 0;

  /// To be called by the VM.
  /// @param cb will be called by the variable implementations when an access
  /// error is encountered.
  virtual void set_access_error_callback(std::function<void()> cb) {
    access_error_cb_ = std::move(cb);
  }

  /// To be called by variables when a read or write fails.
  virtual void report_access_error() {
    if (access_error_cb_) access_error_cb_();
  }

 private:
  std::function<void()> access_error_cb_;
};

} // namespace logic

#endif // _LOGIC_VARIABLE_HXX_
