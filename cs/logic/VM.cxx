#include <stdint.h>
#include "logic/VM.hxx"
#include "logic/Bytecode.hxx"

#include "utils/StringPrintf.hxx"

namespace logic {

void BytecodeStream::append_varint(std::string* output, int value) {
  if (!value) {
    output->push_back(0);
    return;
  } else if (value > 0) {
    uint8_t b = 0;
    b |= value & 0x3F;
    value >>= 6;
    output->push_back(b);
    while (value) {
      output->back() |= 0x80;
      b = value & 0x7F;
      output->push_back(b);
      value >>= 7;
    }
  } else {
    uint8_t b = 0x40;
    b |= value & 0x3F;
    value >>= 6;
    output->push_back(b);
    while (value != -1) {
      output->back() |= 0x80;
      b = value & 0x7F;
      output->push_back(b);
      value >>= 7;
    }
  }
}

bool VM::unexpected_eof(const char* where) {
  error_ = "Unexpected EOF ";
  if (where) error_.append(where);
  return false;
}

bool VM::parse_varint(int* output) {
  int ret = 0;
  if (ip_ >= eof_) {
    return unexpected_eof("parsing varint");
  }
  if ((*ip_) & 0x40) {
    ret = -1;
  }
  ret = (ret & ~0x3F) | ((*ip_) & 0x3F);
  int ofs = 6;
  while (*ip_ & 0x80) {
    ip_++;
    if (ip_ >= eof_) {
      return unexpected_eof("parsing varint");
    }
    ret &= ~(0x7f << ofs);
    ret |= ((*ip_) & 0x7f) << ofs;
    ofs += 7;
  }
  ip_++;
  *output = ret;
  return true;
}

bool VM::execute(const void* data, size_t len) {
  ip_ = (const uint8_t*)data;
  eof_ = ip_ + len;

  while(ip_ <= eof_) {
    switch(*ip_) {
      case TERMINATE:
        return true;
      case PUSH_CONSTANT: {
        ++ip_;
        int r;
        if (!parse_varint(&r)) return false;
        operand_stack_.push_back(r);
        --ip_;
        break;
      }
      case PUSH_CONSTANT_0:
        operand_stack_.push_back(0);
        break;
      case PUSH_CONSTANT_1:
        operand_stack_.push_back(1);
        break;

      case NUMERIC_PLUS: {
        if (operand_stack_.size() < 2) {
          error_ = StringPrintf("Stack underflow at PLUS");
          return false;
        }
        int rhs = operand_stack_.back(); operand_stack_.pop_back();
        int lhs = operand_stack_.back(); operand_stack_.pop_back();
        operand_stack_.push_back(lhs + rhs);
        break;
      }
      case PRINT_NUM: {
        if (operand_stack_.size() < 1) {
          error_ = StringPrintf("Stack underflow at PRINT_NUM");
          return false;
        }
        int arg = operand_stack_.back(); operand_stack_.pop_back();
        print_cb_(StringPrintf("%d", arg));
        break;
      }
      case NOP:
        break;
      default:
        error_ = StringPrintf("Unexpected instruction %02x", *ip_);
        return false;
    }
    ++ip_;
  };
  return true;
};

} // namespace logic
