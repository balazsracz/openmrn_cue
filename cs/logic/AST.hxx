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
#include "logic/location.hh"

/// If this is defined, the bytecode will have extra instructions rendered to
/// double-check the depth of stack when allocating variables.
#define RENDER_STACK_LENGTH_CHECK

namespace logic {

class Driver;

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

  /// Create the VM commands to create a reference to this variable and store
  /// it on top of the operand stack.
  virtual void serialize_ref(std::string* output) = 0;
  
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

  void serialize_ref(std::string* output) override {
    BytecodeStream::append_opcode(output, CREATE_INDIRECT_VAR);
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

  void serialize_ref(std::string* output) override {
    LocalVariableReference::serialize_fetch(output);
  }
  
  const string& get_name() {
    return name_;
  }
};

/// Compound command (aka brace enclosed command sequence).
class CommandSequence : public Command {
 public:
  CommandSequence(std::vector<std::shared_ptr<Command> >&& commands,
                  int trim_stack_from = -1, int trim_stack_to = -1)
      : commands_(std::move(commands)),
        trim_stack_from_(trim_stack_from),
        trim_stack_to_(trim_stack_to) {}

  void serialize(std::string* output) override {
    for (const auto& c : commands_) {
      c->serialize(output);
    }
    if (trim_stack_from_ >= 0) {
#ifdef RENDER_STACK_LENGTH_CHECK    
      BytecodeStream::append_opcode(output, CHECK_STACK_LENGTH);
      BytecodeStream::append_varint(output, trim_stack_from_);
#endif
      if (trim_stack_to_ >= 0) {
        BytecodeStream::append_opcode(output, LEAVE);
        BytecodeStream::append_varint(output, trim_stack_from_ - trim_stack_to_);
      }
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
  /// If non-negative, the stack will be checked to be this size at the end.
  int trim_stack_from_;
  /// If non-negative, the stack will be trimmed to this size at the end.
  int trim_stack_to_;
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
  }

  void debug_print(std::string* output) override {
    output->append(StringPrintf("import_var(%s/guid %d,arg %d)",
                                variable_.get_name().c_str(), guid_, arg_));
  }
  
 private:
  /// Guild of the variable to create and import.
  int guid_;
  /// Argument to supply to the variable access.
  int arg_;
  /// Holds the metadata of the variable.
  GlobalVariableReference variable_;
};

class StaticVarCreate : public Command {
 public:
  StaticVarCreate(std::string name, TypeSpecifier type, int guid, int fp_offset,
                  std::shared_ptr<Command> initial_value)
      : guid_(guid),
        type_(type),
        variable_(name, fp_offset),
        init_(std::move(initial_value)) {}

  void serialize(std::string* output) override {
    BytecodeStream::append_opcode(output, CREATE_STATIC_VAR);
    BytecodeStream::append_varint(output, guid_);
    // Now there are two values on the stack: <bottom> = indirect var
    // offset. <top> = 0 if no init is necessary, 1 if init is necessary.
    string init;
    init_->serialize(&init);
    variable_.serialize_store(&init);
    BytecodeStream::append_opcode(output, TEST_JUMP_IF_FALSE);
    BytecodeStream::append_varint(output, init.size());
    output->append(init);
  }

  void debug_print(std::string* output) override {
    output->append("static ");
    output->append(type_.to_string());
    output->append(" ");
    output->append(variable_.get_name());
    output->append(" = ");
    init_->debug_print(output);
  }
  
 private:
  /// Guild of the variable to create and import.
  int guid_;
  // Variable (syntactic) type.
  TypeSpecifier type_;
  /// Holds the metadata of the variable.
  GlobalVariableReference variable_;
  // Code that initializes the value.
  std::shared_ptr<Command> init_; 
};

class LocalVarCreate : public Command {
 public:
  LocalVarCreate(std::string name, TypeSpecifier type, int fp_offset,
                 std::shared_ptr<Command> initial_value)
      : name_(std::move(name)),
        type_(type),
        fp_offset_(fp_offset),
        init_(std::move(initial_value)) {}

  void debug_print(std::string* output) override {
    output->append(type_.to_string());
    output->append(" ");
    output->append(name_);
    output->append(" = ");
    init_->debug_print(output);
  }
  
  void serialize(std::string* output) override {
#ifdef RENDER_STACK_LENGTH_CHECK
    BytecodeStream::append_opcode(output, CHECK_STACK_LENGTH);
    BytecodeStream::append_varint(output, fp_offset_);
#endif
    init_->serialize(output);
  }
  
 private:
  // Variable name.
  string name_;
  TypeSpecifier type_;
  // FP-relative offset of where the variable should be on the stack.
  int fp_offset_;
  // Code that initializes the value.
  std::shared_ptr<Command> init_; 
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
    output->append(", ");
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
    // ensures we only write 0 or 1 to a boolean variable.
    BytecodeStream::append_opcode(output, BOOL_PROJECT);
    variable_->serialize_store(output);
  }

  void debug_print(std::string* output) override {
    output->append("boolassign(");
    variable_->debug_print(output);
    output->append(", ");
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
      case NUMERIC_MINUS:
        output->append("minus(");
        break;
      case NUMERIC_MUL:
        output->append("mul(");
        break;
      case NUMERIC_DIV:
        output->append("div(");
        break;
      case NUMERIC_MOD:
        output->append("mod(");
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

class IntComp : public BooleanExpression {
 public:
  IntComp(OpCode op, std::shared_ptr<IntExpression> left,
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
      case NUMERIC_EQ:
        output->append("eq(");
        break;
      case NUMERIC_NEQ:
        output->append("noteq(");
        break;
      case NUMERIC_LEQ:
        output->append("leq(");
        break;
      case NUMERIC_GEQ:
        output->append("geq(");
        break;
      case NUMERIC_LT:
        output->append("lt(");
        break;
      case NUMERIC_GT:
        output->append("gt(");
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

class BoolCmp : public BooleanExpression {
 public:
  BoolCmp(OpCode opcode, std::shared_ptr<BooleanExpression> left,
         std::shared_ptr<BooleanExpression> right)
      : opcode_(opcode), left_(std::move(left)), right_(std::move(right)) {
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
      case BOOL_EQ:
        output->append("bool_eq(");
        break;
      case BOOL_NEQ:
        output->append("bool_neq(");
        break;
      default:
        output->append("\?\?\?(");
    }
    left_->debug_print(output);
    output->append(",");
    right_->debug_print(output);
    output->append(")");
  }

 private:
  /// Opcode that performs the given expression on the top of the stack.
  OpCode opcode_;
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
  PolymorphicExpression(Symbol::DataType type,
                        std::unique_ptr<VariableReference> var)
      : var_type_(type), var_(std::move(var)) {}

  PolymorphicExpression() {}

  bool is_void() {
    return !(bool_expr_ || int_expr_);
  }

  bool is_variable() {
    return (bool)var_;
  }

  Symbol::DataType get_data_type() {
    if (bool_expr_) {
      return Symbol::DATATYPE_BOOL;
    } else if (int_expr_) {
      return Symbol::DATATYPE_INT;
    } else if (var_) {
      return var_type_;
    } else {
      return Symbol::DATATYPE_VOID;
    }  
  }
  
  yy::location loc_;
  std::shared_ptr<BooleanExpression> bool_expr_;
  std::shared_ptr<IntExpression> int_expr_;
  Symbol::DataType var_type_ = Symbol::DATATYPE_VOID;
  std::unique_ptr<VariableReference> var_;
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
      if (output->back() != '(') {
        output->append(", ");
      }
      output->append(arg->type_.to_string());
      output->append(" ");
      output->append(arg->name_);
    }
    output->append(") ");
    body_->debug_print(output);
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

class FunctionCall : public Command {
 public:
  typedef std::shared_ptr<
      std::vector<std::shared_ptr<logic::PolymorphicExpression> > >
      ArgsType;
  /// Constructor
  /// @param stack_ofs is an fp-relative offset from whence the operand stack is not used (and can be reused for the purpose of argument passing.
  /// @param name is the function to call
  /// @param fn is the Symbol of the function to call
  /// @param args are the expressions to be passed as function arguments.
  /// @param need_retval true if we need to keep the return value, false if it
  /// needs to be thrown away.
  FunctionCall(unsigned stack_ofs, string name, const Symbol& fn, ArgsType args,
               bool need_retval)
      : stack_ofs_(stack_ofs),
        name_(std::move(name)),
        fn_(fn),
        args_(std::move(args)),
        need_retval_(need_retval) {
    HASSERT(args_);
  }

  void debug_print(std::string* output) override {
    output->append(name_);
    output->append("(");
    bool first = true;
    for (const auto& a: *args_) {
      if (first) {
        first = false;
      } else {
        output->append(", ");
      }
      if (a->int_expr_) {
        a->int_expr_->debug_print(output);
      } else if (a->bool_expr_) {
        a->bool_expr_->debug_print(output);
      }
    }
    output->append(")");
  }

  void serialize(std::string* output) override {
    BytecodeStream::append_opcode(output, CHECK_STACK_LENGTH);
    BytecodeStream::append_varint(output, stack_ofs_);

    // Create space for return value of the function.
    BytecodeStream::append_opcode(output, PUSH_CONSTANT_0);

    // Render arguments to the function.
    unsigned c = 0;
    for (const auto& arg : *args_) {
      if (arg->bool_expr_) {
        arg->bool_expr_->serialize(output);
      } else if (arg->int_expr_) {
        arg->int_expr_->serialize(output);
      } else if (arg->var_) {
        arg->var_->serialize_ref(output);
      } else {
        LOG_ERROR(
            "Error serializing function call %s: arg %c has no expression.",
            name_.c_str(), c);
        BytecodeStream::append_opcode(output, PUSH_CONSTANT_0);
      }
      ++c;
    }

    // Call target address.
    BytecodeStream::append_opcode(output, PUSH_CONSTANT);
    BytecodeStream::append_varint(output, fn_.fp_offset_);

    // Execute call.
    BytecodeStream::append_opcode(output, CALL);
    BytecodeStream::append_varint(output, args_->size());

    if (!need_retval_) {
      // After return: clean up return value.
      BytecodeStream::append_opcode(output, POP_OP);
    }
  }
  
 private:
  /// FP-relative offset which should be the length of the stack.
  int stack_ofs_;
  /// Name of the function we are calling.
  string name_;
  /// The symbol table entry for the function to call.
  const Symbol& fn_;
  /// Argument list.
  ArgsType args_;
  /// true if we need to keep the return value, false if it needs to be thrown
  /// away.
  bool need_retval_;
};

class IntFunctionCall : public IntExpression {
 public:
  template <typename... Args>
  IntFunctionCall(Args&&... args) : fc_(std::forward<Args>(args)...) {}

  void debug_print(std::string* output) override {
    fc_.debug_print(output);
  }

  void serialize(std::string* output) override {
    fc_.serialize(output);
  }
    
 private:
  FunctionCall fc_;
};

class BoolFunctionCall : public BooleanExpression {
 public:
  template <typename... Args>
  BoolFunctionCall(Args&&... args) : fc_(std::forward<Args>(args)...) {}

  void debug_print(std::string* output) override {
    fc_.debug_print(output);
  }

  void serialize(std::string* output) override {
    fc_.serialize(output);
  }
    
 private:
  FunctionCall fc_;
};

class PrintString : public Command {
 public:
  PrintString(string txt) : txt_(std::move(txt)){}

  void debug_print(std::string* output) override {
    output->append("print(\"");
    output->append(txt_);
    output->append("\")");
  }

  void serialize(std::string* output) override {
    BytecodeStream::append_opcode(output, LOAD_STRING);
    BytecodeStream::append_string(output, txt_);
    BytecodeStream::append_opcode(output, PRINT_STR);
  }
  
 private:
  string txt_;
};

class PrintInt : public Command {
 public:
  PrintInt(std::shared_ptr<IntExpression> value) : value_(std::move(value)){}

  void debug_print(std::string* output) override {
    output->append("print_num(");
    value_->debug_print(output);
    output->append(")");
  }

  void serialize(std::string* output) override {
    value_->serialize(output);
    BytecodeStream::append_opcode(output, PRINT_NUM);
  }
  
 private:
  std::shared_ptr<IntExpression> value_;
};

class Terminate : public Command {
 public:
  void serialize(std::string* output) override {
    BytecodeStream::append_opcode(output, TERMINATE);
  };
  void debug_print(std::string* output) override {
    output->append("terminate()");
  };
};

} // namespace logic

#endif // _LOGIC_AST_HXX_
