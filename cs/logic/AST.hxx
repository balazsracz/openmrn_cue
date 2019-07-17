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
 * \file AST.hxx
 *
 * Abstract Syntax Tree for the logic language parser.
 *
 * @author Balazs Racz
 * @date 15 June 2019
 */

#ifndef _LOGIC_AST_HXX_
#define _LOGIC_AST_HXX_

#include <memory>

#include "utils/logging.h"
#include "utils/macros.h"
#include "utils/StringPrintf.hxx"

#include "logic/Bytecode.hxx"

namespace logic {

class Driver;

enum VariableStorageSpecifier {
  LOCAL_VAR,
  INDIRECT_VAR
};

class Command {
 public:
  virtual ~Command() {}
  virtual void serialize(std::string* output) = 0;
  virtual void serialize_preamble(std::string* output) {};
  virtual void debug_print(std::string* output) = 0;
};

class EmptyCommand : public Command {
 public:
  void serialize(std::string* output) override {};
  void debug_print(std::string* output) override {};
};

class VariableReference {
 public:
  /// Create the VM commands to push the variable's value to the operand stack.
  virtual void serialize_fetch(std::string* output) = 0;

  /// Create the VM commands to take the value from the operand stack and store
  /// it into the variable.
  virtual void serialize_store(std::string* output) = 0;

  /// Print the variable representation to the sebug string.
  virtual void debug_print(std::string* output) = 0;
};

class LocalVariableReference : public VariableReference {
 public:
  LocalVariableReference(string name, int fp_offset)
      : name_(std::move(name)), fp_offset_(fp_offset) {}

  void serialize_fetch(std::string* output) override {
    BytecodeStream::append_opcode(output, LOAD_FP_REL);
    BytecodeStream::append_varint(output, fp_offset_);
  }

  /// Create the VM commands to take the value from the operand stack and store
  /// it into the variable.
  void serialize_store(std::string* output) override {
    BytecodeStream::append_opcode(output, STORE_FP_REL);
    BytecodeStream::append_varint(output, fp_offset_);
  }

  /// Print the variable representation to the sebug string.
  void debug_print(std::string* output) {
    output->append(name_);
  }

 protected:
  std::string name_;
  /// FP-relative offset of the variable storage.
  int fp_offset_;
};

class GlobalVariableReference : public LocalVariableReference {
 public:
  GlobalVariableReference(string name, int fp_offset)
      : LocalVariableReference(std::move(name), fp_offset) {}
  
  void serialize_fetch(std::string* output) override {
    LocalVariableReference::serialize_fetch(output);
    BytecodeStream::append_opcode(output, INDIRECT_LOAD);
  }

  void serialize_store(std::string* output) override {
    LocalVariableReference::serialize_fetch(output);
    BytecodeStream::append_opcode(output, INDIRECT_STORE);
  }

  /// Run once when the variable is defined for import. When called, the
  /// variable import result (the offset) is on the top of the
  /// stack. Initializes the internal storage value by pushing it to the right
  /// place in the operand stack.
  void serialize_init(std::string* output) {
    LocalVariableReference::serialize_store(output);
  }
  
  const string& get_name() {
    return name_;
  }
};

/// Compound command (aka brace enclosed command sequence).
class CommandSequence : public Command {
 public:
  CommandSequence(std::vector<std::shared_ptr<Command> > &&commands)
      : commands_(std::move(commands)) {}

  void serialize(std::string* output) override {
    for (const auto& c : commands_) {
      c->serialize(output);
    }
  }

  void debug_print(std::string* output) override {
    output->append("{\n");
    for (const auto& c : commands_) {
      c->debug_print(output);
      output->append("\n");
    }
    output->append("}\n");
  }

 private:
  std::vector<std::shared_ptr<Command> > commands_;
};

class IntExpression : public Command {};

class BooleanExpression : public Command {};

class IndirectVarCreate : public Command {
 public:
  IndirectVarCreate(std::string name, int fp_offset, int guid, int arg = 0)
      : guid_(guid), arg_(arg), variable_(name, fp_offset) {}

  void serialize_preamble(std::string* output) override {
    BytecodeStream::append_opcode(output, LOAD_STRING);
    BytecodeStream::append_string(output, variable_.get_name());
    BytecodeStream::append_opcode(output, CREATE_VAR);
    BytecodeStream::append_varint(output, guid_);
  }

  void serialize(std::string* output) override {
    BytecodeStream::append_opcode(output, PUSH_CONSTANT);
    BytecodeStream::append_varint(output, guid_);
    BytecodeStream::append_opcode(output, PUSH_CONSTANT);
    BytecodeStream::append_varint(output, arg_);
    BytecodeStream::append_opcode(output, IMPORT_VAR);
    variable_.serialize_init(output);
  }

  void debug_print(std::string* output) override {
    output->append(StringPrintf("import_var(%s/guid %d,arg %d)",
                                variable_.get_name(), guid_, arg_));
  }
  
 private:
  /// Guild of the variable to create and import.
  int guid_;
  /// Argument to supply to the variable access.
  int arg_;
  /// Holds the metadata of the variable.
  GlobalVariableReference variable_;
};

class NumericAssignment : public Command {
 public:
  /// Constructs a numeric assignment operation.
  /// @param variable is the abstract reference for the variable.
  /// @param value is the expression that computes the value to be stored
  /// (pushing exactly one entry to the operand stack).
  NumericAssignment(std::unique_ptr<VariableReference> variable,
                    std::shared_ptr<IntExpression> value)
      : variable_(std::move(variable)), value_(std::move(value)) {}

  void serialize(std::string* output) override {
    value_->serialize(output);
    variable_->serialize_store(output);
  }

  void debug_print(std::string* output) override {
    output->append("assign(");
    variable_->debug_print(output);
    output->append(",");
    value_->debug_print(output);
    output->append(")");
  }

 private:
  std::unique_ptr<VariableReference> variable_;
  std::shared_ptr<IntExpression> value_;
};

class IntVariable : public IntExpression {
 public:
  IntVariable(std::unique_ptr<VariableReference> variable)
      : variable_(std::move(variable)) {}

  void serialize(std::string* output) override {
    variable_->serialize_fetch(output);
  }

  void debug_print(std::string* output) override {
    output->append("fetch(");
    variable_->debug_print(output);
    output->append(")");
  }

 private:
  std::unique_ptr<VariableReference> variable_;
};

class BooleanAssignment : public Command {
 public:
  /// Constructs a boolean assignment operation.
  /// @param variable is the abstract reference for the variable.
  /// @param value is the expression that computes the value to be stored
  /// (pushing exactly one entry to the operand stack).
  BooleanAssignment(std::unique_ptr<VariableReference> variable,
                    std::shared_ptr<BooleanExpression> value)
      : variable_(std::move(variable)),
        value_(std::move(value)) {
  }

  void serialize(std::string* output) override {
    value_->serialize(output);
    variable_->serialize_store(output);
  }

  void debug_print(std::string* output) override {
    output->append("boolassign(");
    variable_->debug_print(output);
    output->append(",");
    value_->debug_print(output);
    output->append(")");
  }

 private:
  std::unique_ptr<VariableReference> variable_;
  std::shared_ptr<BooleanExpression> value_;
};

class BoolVariable : public BooleanExpression {
 public:
  BoolVariable(std::unique_ptr<VariableReference> variable)
      : variable_(std::move(variable)) {}

  void serialize(std::string* output) override {
    variable_->serialize_fetch(output);
  }

  void debug_print(std::string* output) override {
    output->append("fetch(");
    variable_->debug_print(output);
    output->append(")");
  }

 private:
  std::unique_ptr<VariableReference> variable_;
};

/// Compound command (aka brace enclosed command sequence).
class IfThenElse : public Command {
 public:
  IfThenElse(std::shared_ptr<BooleanExpression> cond,
             std::shared_ptr<Command> then_case,
             std::shared_ptr<Command> else_case = nullptr)
      : condition_(std::move(cond)),
        then_case_(std::move(then_case)),
        else_case_(std::move(else_case)) {}

  void serialize(std::string* output) override {
    condition_->serialize(output);
    string then_block;
    then_case_->serialize(&then_block);
    string else_block;
    if (else_case_) {
      else_case_->serialize(&else_block);
      BytecodeStream::append_opcode(&then_block, JUMP);
      BytecodeStream::append_varint(&then_block, else_block.size());
    }
    BytecodeStream::append_opcode(output, TEST_JUMP_IF_FALSE);
    BytecodeStream::append_varint(output, then_block.size());
    output->append(then_block);
    output->append(else_block);
  }

  void debug_print(std::string* output) override {
    output->append("if (");
    condition_->debug_print(output);
    output->append(")");
    then_case_->debug_print(output);
    if (else_case_) {
      output->append(" else ");
      else_case_->debug_print(output);
    }
  }

 private:
  /// @todo this should rather be a boolean expression
  std::shared_ptr<BooleanExpression> condition_;
  std::shared_ptr<Command> then_case_;
  std::shared_ptr<Command> else_case_;
};

class IntBinaryExpression : public IntExpression {
 public:
  IntBinaryExpression(OpCode op, std::shared_ptr<IntExpression> left,
                      std::shared_ptr<IntExpression> right)
      : opcode_(op), left_(std::move(left)), right_(std::move(right)) {
    HASSERT(left_);
    HASSERT(right_);
  }

  void serialize(std::string* output) override {
    left_->serialize(output);
    right_->serialize(output);
    BytecodeStream::append_opcode(output, opcode_);
  }

  void debug_print(std::string* output) override {
    switch(opcode_) {
      case NUMERIC_PLUS:
        output->append("plus(");
        break;
      default:
        output->append("?\?\?(");
    }
    left_->debug_print(output);
    output->append(",");
    right_->debug_print(output);
    output->append(")");
  }

 private:
  /// Opcode that performs the given expression on the top of the stack.
  OpCode opcode_;
  std::shared_ptr<IntExpression> left_;
  std::shared_ptr<IntExpression> right_;
};

class IntConstant : public IntExpression {
 public:
  IntConstant(int value) : value_(value) {}

  void serialize(std::string* output) override {
    if (value_ == 0) {
      BytecodeStream::append_opcode(output, PUSH_CONSTANT_0);
    } else if (value_ == 1) {
      BytecodeStream::append_opcode(output, PUSH_CONSTANT_1);
    } else {
      BytecodeStream::append_opcode(output, PUSH_CONSTANT);
      BytecodeStream::append_varint(output, value_);
    }
  }

  void debug_print(std::string* output) override {
    output->append(StringPrintf("%d", value_));
  }

 private:
  int value_;
};

class BooleanConstant : public BooleanExpression {
 public:
  BooleanConstant(bool value) : value_(value) {}

  void serialize(std::string* output) override {
    BytecodeStream::append_opcode(output,
                                  value_ ? PUSH_CONSTANT_1 : PUSH_CONSTANT_0);
  }

  void debug_print(std::string* output) override {
    if (value_) {
      output->append("true");
    } else {
      output->append("false");
    }
  }

 private:
  bool value_;
};

class BoolNot : public BooleanExpression {
 public:
  BoolNot(std::shared_ptr<BooleanExpression> left)
      : left_(std::move(left)) {
    HASSERT(left_);
  }

  void serialize(std::string* output) override {
    left_->serialize(output);
    BytecodeStream::append_opcode(output, BOOL_NOT);
  }

  void debug_print(std::string* output) override {
    output->append("bool_not(");
    left_->debug_print(output);
    output->append(")");
  }

 private:
  std::shared_ptr<BooleanExpression> left_;
};

class BoolAnd : public BooleanExpression {
 public:
  BoolAnd(std::shared_ptr<BooleanExpression> left,
          std::shared_ptr<BooleanExpression> right)
      : left_(std::move(left)), right_(std::move(right)) {
    HASSERT(left_);
    HASSERT(right_);
  }

  void serialize(std::string* output) override {
    left_->serialize(output);
    std::string rhs;
    BytecodeStream::append_opcode(&rhs, POP_OP);
    right_->serialize(&rhs);
    BytecodeStream::append_opcode(output, PUSH_TOP);
    BytecodeStream::append_opcode(output, TEST_JUMP_IF_FALSE);
    BytecodeStream::append_varint(output, rhs.size());
    output->append(rhs);
  }

  void debug_print(std::string* output) override {
    output->append("bool_and(");
    left_->debug_print(output);
    output->append(",");
    right_->debug_print(output);
    output->append(")");
  }

 private:
  std::shared_ptr<BooleanExpression> left_;
  std::shared_ptr<BooleanExpression> right_;
};

class BoolEq : public BooleanExpression {
 public:
  BoolEq(std::shared_ptr<BooleanExpression> left,
          std::shared_ptr<BooleanExpression> right)
      : left_(std::move(left)), right_(std::move(right)) {
    HASSERT(left_);
    HASSERT(right_);
  }

  void serialize(std::string* output) override {
    left_->serialize(output);
    right_->serialize(output);
    BytecodeStream::append_opcode(output, BOOL_EQ);
  }

  void debug_print(std::string* output) override {
    output->append("bool_eq(");
    left_->debug_print(output);
    output->append(",");
    right_->debug_print(output);
    output->append(")");
  }

 private:
  std::shared_ptr<BooleanExpression> left_;
  std::shared_ptr<BooleanExpression> right_;
};

class BoolOr : public BooleanExpression {
 public:
  BoolOr(std::shared_ptr<BooleanExpression> left,
          std::shared_ptr<BooleanExpression> right)
      : left_(std::move(left)), right_(std::move(right)) {
    HASSERT(left_);
    HASSERT(right_);
  }

  void serialize(std::string* output) override {
    left_->serialize(output);
    std::string rhs;
    BytecodeStream::append_opcode(&rhs, POP_OP);
    right_->serialize(&rhs);
    BytecodeStream::append_opcode(output, PUSH_TOP);
    BytecodeStream::append_opcode(output, TEST_JUMP_IF_TRUE);
    BytecodeStream::append_varint(output, rhs.size());
    output->append(rhs);
  }

  void debug_print(std::string* output) override {
    output->append("bool_or(");
    left_->debug_print(output);
    output->append(",");
    right_->debug_print(output);
    output->append(")");
  }

 private:
  std::shared_ptr<BooleanExpression> left_;
  std::shared_ptr<BooleanExpression> right_;
};

struct PolymorphicExpression {
  PolymorphicExpression(std::shared_ptr<BooleanExpression> && bool_expr)
      : bool_expr_(std::move(bool_expr)) {}
  PolymorphicExpression(std::shared_ptr<IntExpression> && int_expr)
      : int_expr_(std::move(int_expr)) {}
  PolymorphicExpression() {}

  bool is_void() {
    return !(bool_expr_ || int_expr_);
  }

  Symbol::DataType get_data_type() {
    if (bool_expr_) {
      return Symbol::DATATYPE_BOOL;
    } else if (int_expr_) {
      return Symbol::DATATYPE_INT;
    } else {
      return Symbol::DATATYPE_VOID;
    }  
  }
  
  std::shared_ptr<BooleanExpression> bool_expr_;
  std::shared_ptr<IntExpression> int_expr_;
};

/// Represents a variable as the argument to a function.
class FunctionArgument {
 public:
  FunctionArgument(string name, logic::TypeSpecifier type)
      : name_(std::move(name))
      , type_(std::move(type)) {}

  std::string name_;
  logic::TypeSpecifier type_;
};

/// Represents a function definition. (This is the code, not the actual call of
/// the function).
class Function : public Command {
 public:
  Function(
      Driver* driver, string name, logic::TypeSpecifier return_type,
      std::shared_ptr<std::vector<std::shared_ptr<FunctionArgument> > > args,
      std::shared_ptr<CommandSequence> body)
      : driver_(driver),
        name_(std::move(name)),
        return_type_(std::move(return_type)),
        args_(std::move(args)),
        body_(std::move(body)) {}

  void serialize(std::string* output) override {
    string tmp;
    body_->serialize(&tmp);
    BytecodeStream::append_opcode(&tmp, RET);

    // Jump over the function body.
    BytecodeStream::append_opcode(output, JUMP);
    BytecodeStream::append_varint(output, tmp.size());

    register_function(output);
    output->append(tmp);
  };

  /// Add the function target address to the driver's jump table.
  /// @param output is the 
  void register_function(std::string* output);

  void debug_print(std::string* output) override {
    output->append(return_type_.to_string());
    output->append(" ");
    output->append(name_);
    output->append("(");
    for (const auto& arg: *args_) {
      output->append(arg->type_.to_string());
      output->append(" ");
      output->append(arg->name_);
    }
    output->append(") {\n");
    body_->debug_print(output);
    output->append("}\n");
  };

  // The compiler driver.
  Driver* driver_;
  // Name of the function.
  string name_;
  // Return type.
  TypeSpecifier return_type_;
  // Arguments
  std::shared_ptr<std::vector<std::shared_ptr<FunctionArgument> > > args_;
  // Commands to execute inside this function.
  std::shared_ptr<CommandSequence> body_;
};

} // namespace logic

#endif // _LOGIC_AST_HXX_
