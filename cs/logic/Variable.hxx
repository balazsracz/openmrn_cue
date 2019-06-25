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


/// This structure is used to parametrize the variable creation interface.
struct VariableCreationRequest {
  VariableCreationRequest() {
    clear();
  }
  void clear() {
    name.clear();
    defaultEvent = 0;
    defaultOnEvent = 0;
  }
  /// User visible name of the variable (may be syntactical name or a string
  /// provided by the code author).
  string name;
  /// If a variable is newly created, and this value is not zero, then it will
  /// be initialized to this event ID. This is the base event ID (for
  /// multiplicity) or the OFF state for binary.
  uint64_t defaultEvent;
  /// If a boolean variable is newly created, and this value is not zero, then
  /// the new variable will be initialized to this as being active / on event
  /// ID.
  uint64_t defaultOnEvent;
};

class Variable {
 public:
  virtual ~Variable() {}
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
};


#endif // _LOGIC_VARIABLE_HXX_
