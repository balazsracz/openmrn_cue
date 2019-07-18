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
 * \file VM.cxx
 *
 * Implementation of the virtual machine 
 *
 * @author Balazs Racz
 * @date 15 June 2019
 */

#include <stdint.h>
#include "logic/VM.hxx"
#include "logic/Bytecode.hxx"

#include "utils/StringPrintf.hxx"
#include "utils/logging.h"

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

bool VM::parse_string() {
  int len;
  string_acc_.clear();
  if (!parse_varint(&len)) {
    return false;
  }
  if (ip_ + len > eof_) {
    return unexpected_eof("parsing string");
  }
  string_acc_.assign((const char*)ip_, len);
  ip_ += len;
  return true;
}

bool VM::execute(const void* data, size_t len) {
  ip_start_ = ip_ = (const uint8_t*)data;
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
      case ENTER: {
        ++ip_;
        int r;
        if (!parse_varint(&r)) return false;
        operand_stack_.resize(operand_stack_.size() + r);
        --ip_;
        break;
      }
      case CHECK_STACK_LENGTH: {
        ++ip_;
        int r;
        if (!parse_varint(&r)) return false;
        --ip_;
        if (operand_stack_.size() != (unsigned)r) {
          error_ = StringPrintf(
              "At IP %u: Operand stack length error, expected %d, actual %u.",
              (unsigned)(ip_ - ip_start_), r, operand_stack_.size());
          return false;
        }
        break;
      }
      case STORE_FP_REL: {
        if (operand_stack_.size() < 1) {
          error_ = StringPrintf("Stack underflow at STORE_FP_REL");
          return false;
        }
        int val = operand_stack_.back(); operand_stack_.pop_back();
        int relofs;
        ++ip_;
        if (!parse_varint(&relofs)) return false;
        --ip_;
        unsigned ofs = fp_ + relofs;
        if (ofs >= operand_stack_.size()) {
          error_ = "Invalid relative offset for STORE_FP_REL";
          return false;
        }
        operand_stack_[ofs] = val;
        break;
      }
      case LOAD_FP_REL: {
        int relofs;
        ++ip_;
        if (!parse_varint(&relofs)) return false;
        --ip_;
        unsigned ofs = fp_ + relofs;
        if (ofs >= operand_stack_.size()) {
          error_ = "Invalid relative offset for LOAD_FP_REL";
          return false;
        }
        int val = operand_stack_[ofs];
        operand_stack_.push_back(val);
        break;
      }
      case INDIRECT_LOAD: {
        if (operand_stack_.size() < 1) {
          error_ = StringPrintf("Stack underflow at INDIRECT_LOAD");
          return false;
        }
        int varidx = operand_stack_.back(); operand_stack_.pop_back();
        if (varidx < 0 || (unsigned)varidx >= variable_stack_.size()) {
          error_ = "Invalid indirect variable reference.";
          return false;
        }
        const VariableReference& ref = variable_stack_[varidx];
        int val = ref.var->read(variable_factory_, ref.arg);
        operand_stack_.push_back(val);
        break;
      }
      case INDIRECT_STORE: {
        if (operand_stack_.size() < 2) {
          error_ = StringPrintf("Stack underflow at INDIRECT_LOAD");
          return false;
        }
        int varidx = operand_stack_.back(); operand_stack_.pop_back();
        int value = operand_stack_.back(); operand_stack_.pop_back();
        if (varidx < 0 || (unsigned)varidx >= variable_stack_.size()) {
          error_ = "Invalid indirect variable reference.";
          return false;
        }
        const VariableReference& ref = variable_stack_[varidx];
        ref.var->write(variable_factory_, ref.arg, value);
        break;
      }
      case PUSH_TOP: {
        if (operand_stack_.size() < 1) {
          error_ = StringPrintf("Stack underflow at PUSH_TOP");
          return false;
        }
        operand_stack_.push_back(operand_stack_.back());
        break;
      }
      case POP_OP: {
        if (operand_stack_.size() < 1) {
          error_ = StringPrintf("Stack underflow at POP_OP");
          return false;
        }
        operand_stack_.pop_back();
        break;
      }
      case PUSH_CONSTANT_0:
        operand_stack_.push_back(0);
        break;
      case PUSH_CONSTANT_1:
        operand_stack_.push_back(1);
        break;
      case IF_PREAMBLE:
        operand_stack_.push_back(is_preamble_ ? 1 : 0);
        break;
      case LOAD_STRING:
        ip_++;
        if (!parse_string()) {
          return false;
        }
        ip_--;
        break;
      case CREATE_VAR: {
        int guid;
        ++ip_;
        if (!parse_varint(&guid)) return false;
        --ip_;
        variable_request_.name = std::move(string_acc_);
        variable_request_.block_num = block_num_;
        auto var = variable_factory_->create_variable(&variable_request_);
        variable_request_.clear();
        external_variables_[guid] = std::move(var);
        break;
      }
      case IMPORT_VAR: {
        if (operand_stack_.size() < 2) {
          error_ = StringPrintf("Stack underflow at IMPORT_VAR");
          return false;
        }
        int arg = operand_stack_.back(); operand_stack_.pop_back();
        int guid = operand_stack_.back(); operand_stack_.pop_back();
        auto it = external_variables_.find(guid);
        if (it == external_variables_.end()) {
          error_ = StringPrintf("Unknown variable GUID %d at IMPORT_VAR", guid);
          return false;
        }
        VariableReference ref;
        ref.var = it->second.get();
        ref.arg = arg;
        variable_stack_.emplace_back(std::move(ref));
        operand_stack_.push_back(variable_stack_.size() - 1);
        break;
      }
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
      case NUMERIC_MINUS: {
        if (operand_stack_.size() < 2) {
          error_ = StringPrintf("Stack underflow at MINUS");
          return false;
        }
        int rhs = operand_stack_.back(); operand_stack_.pop_back();
        int lhs = operand_stack_.back(); operand_stack_.pop_back();
        operand_stack_.push_back(lhs - rhs);
        break;
      }
      case NUMERIC_MUL: {
        if (operand_stack_.size() < 2) {
          error_ = StringPrintf("Stack underflow at MUL");
          return false;
        }
        int rhs = operand_stack_.back(); operand_stack_.pop_back();
        int lhs = operand_stack_.back(); operand_stack_.pop_back();
        operand_stack_.push_back(lhs * rhs);
        break;
      }
      case NUMERIC_DIV: {
        if (operand_stack_.size() < 2) {
          error_ = StringPrintf("Stack underflow at DIV");
          return false;
        }
        int rhs = operand_stack_.back(); operand_stack_.pop_back();
        int lhs = operand_stack_.back(); operand_stack_.pop_back();
        if (rhs == 0) {
          error_ = StringPrintf("Div by zero");
          return false;
        }
        operand_stack_.push_back(lhs / rhs);
        break;
      }
      case NUMERIC_MOD: {
        if (operand_stack_.size() < 2) {
          error_ = StringPrintf("Stack underflow at MOD");
          return false;
        }
        int rhs = operand_stack_.back(); operand_stack_.pop_back();
        int lhs = operand_stack_.back(); operand_stack_.pop_back();
        if (rhs == 0) {
          error_ = StringPrintf("Div by zero");
          return false;
        }
        operand_stack_.push_back(lhs % rhs);
        break;
      }
      case BOOL_EQ: {
        if (operand_stack_.size() < 2) {
          error_ = StringPrintf("Stack underflow at BOOL_EQ");
          return false;
        }
        int rhs = operand_stack_.back(); operand_stack_.pop_back();
        rhs = !!rhs;
        int lhs = operand_stack_.back(); operand_stack_.pop_back();
        lhs = !!lhs;
        operand_stack_.push_back(lhs == rhs);
        break;
      }
      case BOOL_NOT: {
        if (operand_stack_.size() < 1) {
          error_ = StringPrintf("Stack underflow at NOT");
          return false;
        }
        int rhs = operand_stack_.back(); operand_stack_.pop_back();
        if (rhs == 0) {
          operand_stack_.push_back(1);
        } else {
          operand_stack_.push_back(0);
        }
        break;
      }

      case JUMP: {
        ++ip_;
        int r;
        if (!parse_varint(&r)) return false;
        ip_ += r;
        --ip_;
        break;
      }
      case CALL: {
        if (operand_stack_.size() < 1) {
          error_ = StringPrintf("Stack underflow at CALL");
          return false;
        }
        if (call_stack_.size() < 1) {
          error_ = StringPrintf("Call stack underflow at CALL");
          return false;
        }
        int dst = operand_stack_.back(); operand_stack_.pop_back();
        ++ip_;
        int num_arg;
        if (!parse_varint(&num_arg)) return false;
        --ip_;
        if (fp_ != call_stack_.back().fp) {
          error_ = StringPrintf("Unexpected fp at CALL. Expected %d actual %d",
                                call_stack_.back().fp, fp_);
          return false;
        }
        call_stack_.emplace_back();
        call_stack_.back().return_address = ip_;
        call_stack_.back().fp = operand_stack_.size() - num_arg;
        fp_ = call_stack_.back().fp;
        call_stack_.back().vp = variable_stack_.size();
        LOG(INFO, "executing CALL. return address %u, dst %u", (unsigned)(call_stack_.back().return_address - ip_start_), (unsigned)dst);
        ip_ = ip_start_ + dst;
        --ip_; // compensate for ++ip at the end ofthis function.
        break;
      }
      case RET: {
        if (call_stack_.size() <= 1) {
          error_ = StringPrintf("Call stack underflow at RET");
          return false;
        }
        const auto& s = call_stack_.back();
        variable_stack_.resize(s.vp);
        operand_stack_.resize(s.fp);
        ip_ = s.return_address;
        call_stack_.pop_back();
        const auto& ss = call_stack_.back();
        fp_ = ss.fp;
        break;
      }
      case TEST_JUMP_IF_FALSE: {
        if (operand_stack_.size() < 1) {
          error_ = StringPrintf("Stack underflow at TEST_JUMP_IF_FALSE");
          return false;
        }
        ++ip_;
        int r;
        if (!parse_varint(&r)) return false;
        int arg = operand_stack_.back();
        operand_stack_.pop_back();
        if (arg == 0) {
          ip_ += r;
        }
        --ip_;
        break;
      }
      case TEST_JUMP_IF_TRUE: {
        if (operand_stack_.size() < 1) {
          error_ = StringPrintf("Stack underflow at TEST_JUMP_IF_TRUE");
          return false;
        }
        ++ip_;
        int r;
        if (!parse_varint(&r)) return false;
        int arg = operand_stack_.back();
        operand_stack_.pop_back();
        if (arg != 0) {
          ip_ += r;
        }
        --ip_;
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
      case PRINT_STR: {
        print_cb_(string_acc_);
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
