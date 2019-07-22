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
 * \file Bytecode.hxx
 *
 * Bytecode definiteions and parser/serializer.
 *
 * @author Balazs Racz
 * @date 15 June 2019
 */

#ifndef _LOGIC_BYTECODE_HXX_
#define _LOGIC_BYTECODE_HXX_

#include <stdint.h>
#include <inttypes.h>
#include <string>
#include <memory>

#include "utils/macros.h"

namespace logic {

struct FunctionArgument;

typedef std::shared_ptr<std::vector<std::shared_ptr<FunctionArgument> > >
    FunctionArgList;

/// An entry in the symbol table.
struct Symbol {
  enum Type {
    /// Variable
    VARIABLE,
    /// Function
    FUNCTION,
  };

  enum DataType {
    DATATYPE_VOID,
    DATATYPE_INT,
    DATATYPE_BOOL,
  };

  enum Access {
    LOCAL_VAR,
    INDIRECT_VAR,
    STATIC_VAR
  };

  DataType get_data_type() const {
    return data_type_;
  }

  Access get_access() const {
    return access_;
  }

  static const char* datatype_to_string(DataType d) {
    switch(d) {
      case DATATYPE_VOID: return "void";
      case DATATYPE_INT: return "int";
      case DATATYPE_BOOL: return "bool";
      default:
        return "?\?\?";
    }
  }
  
  /// type of this symbol.
  Type symbol_type_ : 8;
  /// For variables, how do we access it.
  Access access_ : 8;
  /// What is the data type of this symbol.
  DataType data_type_ : 8;
  /// relative offset on the operand stack from the fp. Negative for a function
  /// in case the
  int fp_offset_{-1};
  /// If this is a function, argument list to it.
  std::weak_ptr<typename FunctionArgList::element_type> args_;  
  /// @todo add declaration location.
};

struct TypeSpecifier {
  Symbol::DataType builtin_type_;

  string to_string() const {
    return Symbol::datatype_to_string(builtin_type_);    
  }
};

/// Represents a variable as the argument to a function.
struct FunctionArgument {
  FunctionArgument(string name, logic::TypeSpecifier type,
                   Symbol::Access access = Symbol::LOCAL_VAR)
      : name_(std::move(name)), type_(std::move(type)), access_(access) {}

  std::string name_;
  logic::TypeSpecifier type_;
  Symbol::Access access_;
};

enum OpCode : uint8_t {
  TERMINATE = 0,
  PUSH_CONSTANT,
  PUSH_CONSTANT_0,
  PUSH_CONSTANT_1,
  // Duplicates the value on the top of the stack.
  PUSH_TOP,
  // Removes the top of the operand stack (throwaway)
  POP_OP,
  // Pushes zeros to the stack. Arg = how many zeros to push.
  ENTER,
  // Removes a certain number of elements from the top of the stack. Arg = how
  // many to remove.
  LEAVE,
  // Verifies that the length of the operand stack is as expected. One varint
  // argument arg. The operand stack's length shall be fp_ + arg, otherwise the
  // VM terminates.
  CHECK_STACK_LENGTH,
  // Pops the stack and writes the value an fp-relative position on the operand
  // stack. Arg=fp-relative offset of where to write the value.
  STORE_FP_REL,
  // Loads a value from the operand stack from an fp-relative position, and
  // pushes it to the stack. Arg=fp-relative offset of where to read the value
  // from.
  LOAD_FP_REL,

  /// Takes a value from the top of the stack, interprets it as a variable
  /// offset in the variable stack, then uses that variable to load a value,
  /// which is pushed to the stack.
  INDIRECT_LOAD,

  /// Takes two values off the stack (lhs, rhs). Interprets rhs as a variable
  /// offset in the variable stack. Writes lhs to this variable.
  INDIRECT_STORE,

  ASSIGN_VAR,
  ASSIGN_VAR_0,
  ASSIGN_VAR_1,

  // Takes a string argument from the bytecode and loads the string accumulator
  // with it. Format=one varint describing the length, then the raw bytes.
  LOAD_STRING,

  /// Invokes the variable factory to create a variable. Uses the string
  /// accumulator for the variable name. arg = GUID of the variable (under
  /// which it will be stored). arg2 = -1 for bool variable, >= 2 for an int
  /// variable (of arg2 states).
  CREATE_VAR,

  /// Creates a static variable. arg = GUID of the variable (under which it
  /// will be stored). The variable will be entered to the variable stack
  /// (equivalent to IMPORT), and the offset is pushed onto the operand
  /// stack. Another value will be pushed to the op stack, 0 if there was not
  /// yet a variable under this GUID and it was newly created, or 0 if the
  /// variable aready existed.
  CREATE_STATIC_VAR,
  
  /// Imports a variable. On the operand stack lhs = GUID of variable; rhs =
  /// argument. The created variable is pushed to the variable stack. The
  /// offset in the variable stack for the new variable is pushed to the
  /// operand stack.
  IMPORT_VAR,

  /// arg = fp-relative offset of a local variable. Creates a new variable
  /// pointing to the same storage location and pushes it to the variable
  /// stack. The offset in the variable stack is pushed to the operand stack.
  CREATE_INDIRECT_VAR,
  
  // Binary arithmetic operators. Take two values from the top of the stack, and
  // push one. The RHS is the top of the stack, the LHS is the second top.
  NUMERIC_PLUS,
  NUMERIC_MINUS,
  NUMERIC_MUL,
  NUMERIC_DIV,
  NUMERIC_MOD,
  BOOL_EQ,
  BOOL_NEQ,

  NUMERIC_LEQ,
  NUMERIC_GEQ,
  NUMERIC_LT,
  NUMERIC_GT,
  NUMERIC_EQ,
  NUMERIC_NEQ,

  // Unary arithmetic operators. They act on the top of the stack.
  NUMERIC_INVERT,
  BOOL_NOT,
  // turns a value on the top of the stack to a bool (0 or 1).
  BOOL_PROJECT,
  
  // Branching

  // Pushes 0 to the stack if the VM is running the preamble right now, else
  // pushes 1.
  IF_PREAMBLE,

  // Unconditional jump. Reads an argument (a varint), then jumps that many
  // bytes with the IP.
  JUMP,

  /// Enter a subroutine. Pops destination address from the operand stack. Then
  /// takes argument arg, creates a new stack frame in the execution
  /// environment, and sets FP to the current operand stack length minus
  /// arg. Essentially arg is the number of arguments passed to the function
  /// via the operand stack. Then jumps to the presented address.
  CALL,

  /// Return from a subroutine. TODO: spec
  RET,
  
  // Takes the top of the opstack (removes it), takes a varint argument as a
  // relative offset, and if the operand is zero, jumps to that relative
  // offset.
  TEST_JUMP_IF_FALSE,
  // same inverted.
  TEST_JUMP_IF_TRUE,

  // Takes the top of stack and prints it to the "output"
  PRINT_NUM,

  // Takes the string accumulator and prints it to the "output"
  PRINT_STR,
  
  NOP = 0xff,
};

struct BytecodeStream {
  /// Appends a varint encoding value to a string.
  static void append_varint(std::string* output, int value);
  /// Appends an opcode to a string.
  static void append_opcode(std::string* output, OpCode opcode) {
    output->push_back(opcode);
  }
  static void append_string(std::string* output, const std::string& value) {
    append_varint(output, value.size());
    output->append(value);
  }
};

} // namespace logic


#endif // _LOGIC_BYTECODE_HXX_
