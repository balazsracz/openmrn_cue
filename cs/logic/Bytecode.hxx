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
#include <string>

#include "utils/macros.h"

namespace logic {

/// An entry in the symbol table.
struct Symbol {
  enum Type {
    /// Variable allocated on the operand stack of type 'int'.
    LOCAL_VAR_INT,
    /// Variable allocated on the operand stack of type 'bool'.
    LOCAL_VAR_BOOL,
    /// External or indirect variable of type 'int'.
    INDIRECT_VAR_INT,
    /// External or indirect variable of type 'bool'.
    INDIRECT_VAR_BOOL,
  };

  enum DataType {
    DATATYPE_INT,
    DATATYPE_BOOL,
  };

  enum Access {
    LOCAL_VAR,
    INDIRECT_VAR
  };

  DataType get_data_type() const {
    switch (symbol_type_) {
      case LOCAL_VAR_INT:
      case INDIRECT_VAR_INT:
        return DATATYPE_INT;
      case LOCAL_VAR_BOOL:
      case INDIRECT_VAR_BOOL:
        return DATATYPE_BOOL;
      default:
        DIE("Unexpected symbol type.");
    }
  }

  Access get_access() const {
    switch (symbol_type_) {
      case LOCAL_VAR_INT:
      case LOCAL_VAR_BOOL:
        return LOCAL_VAR;
      case INDIRECT_VAR_INT:
      case INDIRECT_VAR_BOOL:
        return INDIRECT_VAR;
      default:
        DIE("Unexpected symbol type.");
    }
  }

  static const char* datatype_to_string(DataType d) {
    switch(d) {
      case DATATYPE_INT: return "int";
      case DATATYPE_BOOL: return "bool";
      default:
        return "?\?\?";
    }
  }
  
  /// type of this symbol.
  Type symbol_type_;
  /// relative offset on the operand stack from the fp.
  int fp_offset_;
  /// @todo add declaration location.
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
  /// which it will be stored).
  CREATE_VAR,

  /// Imports a variable. On the operand stack lhs = GUID of variable; rhs =
  /// argument. The created variable is pushed to the variable stack. The
  /// offset in the variable stack for the new variable is pushed to the
  /// operand stack.
  IMPORT_VAR,
  
  // Binary arithmetic operators. Take two values from the top of the stack, and
  // push one. The RHS is the top of the stack, the LHS is the second top.
  NUMERIC_PLUS,
  NUMERIC_MINUS,
  NUMERIC_MUL,
  NUMERIC_DIV,
  NUMERIC_MOD,
  BOOL_EQ,

  // Unary arithmetic operators. They act on the top of the stack.
  NUMERIC_INVERT,
  BOOL_NOT,
  
  // Branching

  // Pushes 0 to the stack if the VM is running the preamble right now, else
  // pushes 1.
  IF_PREAMBLE,

  // Unconditional jump. Reads an argument (a varint), then jumps that many
  // bytes with the IP.
  JUMP,

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
