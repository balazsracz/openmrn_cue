#ifndef _LOGIC_BYTECODE_HXX_
#define _LOGIC_BYTECODE_HXX_

#include <stdint.h>
#include <string>

namespace logic {

enum OpCode : uint8_t {
  TERMINATE = 0,
  PUSH_CONSTANT,
  PUSH_CONSTANT_0,
  PUSH_CONSTANT_1,
  ASSIGN_VAR,
  ASSIGN_VAR_0,
  ASSIGN_VAR_1,

  // Binary arithmetic operators. Take two values from the top of the stack, and
  // push one. The RHS is the top of the stack, the LHS is the second top.
  NUMERIC_PLUS,
  NUMERIC_MINUS,
  NUMERIC_MUL,
  NUMERIC_DIV,
  NUMERIC_MOD,

  // Unary arithmetic operators. They act on the top of the stack.
  NUMERIC_INVERT,

  // Branching
  
  // Unconditional jump. Reads an argument (a varint), then jumps that many
  // bytes with the IP.
  JUMP,
  
  // Takes the top of the opstack (removes it), takes a varint argument as a
  // relative offset, and if the operand is zero, jumps to that relative
  // offset.
  TEST_JUMP_IF_FALSE,
  
  // Takes the top of stack and prints it to the "output"
  PRINT_NUM,

  NOP = 0xff,
};

struct BytecodeStream {
  /// Appends a varint encoding value to a string.
  static void append_varint(std::string* output, int value);
  /// Appends an opcode to a string.
  static void append_opcode(std::string* output, OpCode opcode) {
    output->push_back(opcode);
  }
};

} // namespace logic


#endif // _LOGIC_BYTECODE_HXX_
