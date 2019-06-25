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
 * \file MockVariable.hxx
 *
 * Helper classes for testing variables.
 *
 * @author Balazs Racz
 * @date 25 June 2019
 */

#ifndef _LOGIC_MOCKVARIABLE_HXX_
#define _LOGIC_MOCKVARIABLE_HXX_

#include "logic/Variable.hxx"

#include <queue>

using ::testing::StrictMock;

namespace logic {

/// Mock implementation of a variable that can have expectations.
class MockVariable : public Variable {
 public:
  MOCK_METHOD0(max_state, int ());
  MOCK_METHOD2(read, int(const VariableFactory* parent, uint16_t arg));
  MOCK_METHOD3(write, void(const VariableFactory* parent, uint16_t arg, int value));
};

/// Variable factory that creates StrictMock variables.
class MockVariableFactory : public VariableFactory {
 public:
  std::unique_ptr<Variable> create_variable(
      VariableCreationRequest* request) const override {
    next_variable(request->name);
    if (expected_variables_.empty()) {
      EXPECT_TRUE(false) << "Ran out of expected variables.";
      return nullptr;
    }
    auto p = std::move(expected_variables_.front());
    expected_variables_.pop();
    return p;
  }

  template<class Matcher>
  StrictMock<MockVariable>* expect_variable(const Matcher& m) const {
    StrictMock<MockVariable>* r = new StrictMock<MockVariable>();
    EXPECT_CALL(*this, next_variable(m));
    expected_variables_.emplace(r);
    return r;
  }

  MOCK_CONST_METHOD1(next_variable, void(const string&));
  
  /// The variable we expect to be created by the system.
  mutable std::queue<std::unique_ptr<Variable> > expected_variables_;
};

} // namespace logic


#endif // _LOGIC_MOCKVARIABLE_HXX_
