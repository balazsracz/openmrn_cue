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

void VM::save_variables(unsigned begin, unsigned end) {
  auto it = external_variables_.lower_bound(begin);
  // yes we need the lower bound again, because end is already open
  // (excluding).
  auto it_end = external_variables_.lower_bound(end);
  while (it != it_end) {
    external_variable_holding_.emplace_back(std::move(it->second));
    it = external_variables_.erase(it);
  }
}

void VM::destroy_saved_variables() {
  external_variable_holding_.clear();
}

/// @return the IP pointing to the start of the current block.
VM::ip_t VM::get_block_start_ip() {
  return block_num_ << BLOCK_CODE_IP_SHIFT;
}

/// @return the current IP.
inline VM::ip_t VM::get_ip() {
  return (_ip_block_num_ << BLOCK_CODE_IP_SHIFT) | (_ip_ - _block_start_);
}

/// @return the IP pointing to the start of the current block.
inline bool VM::at_eof() {
  return _ip_ >= _eof_;
}

inline uint8_t VM::fetch_insn() {
  return *_ip_++;
}

bool VM::jump(ip_t target) {
  unsigned block_num = target >> BLOCK_CODE_IP_SHIFT;
  if (block_num >= blocks_.size()) return false;
  unsigned ofs = target & ((1u<<BLOCK_CODE_IP_SHIFT) - 1);
  const auto& c = blocks_[block_num].code_;
  if (ofs > c.size()) {
    return false;
  }
  _ip_block_num_ = block_num;
  _block_start_ = (const uint8_t*) (c.data());
  _eof_ = (const uint8_t*) (c.data() + c.size());
  _ip_ = (const uint8_t*) (c.data() + ofs);
  return true;
}

bool VM::unexpected_eof(const char* where) {
  error_ = "Unexpected EOF ";
  if (where) error_.append(where);
  return false;
}

void VM::access_error() {
  access_error_ = 1;
  error_ = StringPrintf("At IP %u: Variable access error.",
                        get_ip());
}


bool VM::parse_varint(int* output) {
  int ret = 0;
  if (_ip_ >= _eof_) {
    return unexpected_eof("parsing varint");
  }
  if ((*_ip_) & 0x40) {
    ret = -1;
  }
  ret = (ret & ~0x3F) | ((*_ip_) & 0x3F);
  int ofs = 6;
  while (*_ip_ & 0x80) {
    _ip_++;
    if (_ip_ >= _eof_) {
      return unexpected_eof("parsing varint");
    }
    ret &= ~(0x7f << ofs);
    ret |= ((*_ip_) & 0x7f) << ofs;
    ofs += 7;
  }
  _ip_++;
  *output = ret;
  return true;
}

bool VM::parse_string() {
  int len;
  string_acc_.clear();
  if (!parse_varint(&len)) {
    return false;
  }
  if (_ip_ + len > _eof_) {
    return unexpected_eof("parsing string");
  }
  string_acc_.assign((const char*)_ip_, len);
  _ip_ += len;
  return true;
}

//ip_start_ = ip_ = (const uint8_t*)data;
//  eof_ = ip_ + len;

#define GET_VARINT(name) \
  int name;              \
  if (!parse_varint(&name)) return false;

#define GET_FROM_STACK(name, INSN)                                           \
  if (operand_stack_.empty()) {                                              \
    error_ = StringPrintf("Stack underflow at ip %u insn %02x %s", get_ip(), \
                          insn, INSN);                                       \
    return false;                                                            \
  }                                                                          \
  int name = operand_stack_.back();                                          \
  operand_stack_.pop_back();

bool VM::execute() {
  while(!at_eof()) {
    volatile uint8_t insn = fetch_insn();
    switch(insn) {
      case TERMINATE:
        return true;
      case PUSH_CONSTANT: {
        GET_VARINT(r);
        operand_stack_.push_back(r);
        break;
      }
      case ENTER: {
        GET_VARINT(r);
        operand_stack_.resize(operand_stack_.size() + r);
        break;
      }
      case LEAVE: {
        GET_VARINT(r);
        if (r < 0) {
          error_ = StringPrintf(
              "At IP %u: LEAVE with negative argument %d.",
              get_ip(), r);
          return false;
        }
        if (r > (int)operand_stack_.size()) {
          error_ = StringPrintf(
              "At IP %u: Stack underflow with LEAVE; arg=%d stack size=%u",
              get_ip(), r, (int)operand_stack_.size());
          return false;
        }
        operand_stack_.resize(operand_stack_.size() - r);
        break;
      }
      case CHECK_STACK_LENGTH: {
        GET_VARINT(r);
        if (operand_stack_.size() != (unsigned)(fp_ + r)) {
          error_ = StringPrintf(
              "At IP %u: Operand stack length error, expected %d, actual %u.",
              get_ip(), r, (unsigned)operand_stack_.size());
          return false;
        }
        break;
      }
      case STORE_FP_REL: {
        GET_FROM_STACK(val, "STORE_FP_REL");
        GET_VARINT(relofs);
        unsigned ofs = fp_ + relofs;
        if (ofs >= operand_stack_.size()) {
          error_ = "Invalid relative offset for STORE_FP_REL";
          return false;
        }
        operand_stack_[ofs] = val;
        break;
      }
      case LOAD_FP_REL: {
        GET_VARINT(relofs);
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
        GET_FROM_STACK(varidx, "INDIRECT_LOAD");
        if (varidx < 0 || (unsigned)varidx >= variable_stack_.size()) {
          error_ = "Invalid indirect variable reference.";
          return false;
        }
        const VMVariableReference& ref = variable_stack_[varidx];
        int val = ref.var->read(variable_factory_, ref.arg);
        if (access_error_) return false;
        operand_stack_.push_back(val);
        break;
      }
      case INDIRECT_STORE: {
        GET_FROM_STACK(varidx, "INDIRECT_STORE");
        GET_FROM_STACK(value, "INDIRECT_STORE");
        if (varidx < 0 || (unsigned)varidx >= variable_stack_.size()) {
          error_ = "Invalid indirect variable reference.";
          return false;
        }
        const VMVariableReference& ref = variable_stack_[varidx];
        ref.var->write(variable_factory_, ref.arg, value);
        if (access_error_) return false;
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
        if (!parse_string()) {
          return false;
        }
        break;
      case CREATE_VAR: {
        GET_VARINT(guid);
        variable_request_.name = std::move(string_acc_);
        variable_request_.block_num = block_num_;
        auto var = variable_factory_->create_variable(&variable_request_);
        variable_request_.clear();
        external_variables_[guid] = std::move(var);
        break;
      }
      case IMPORT_VAR: {
        GET_FROM_STACK(arg, "IMPORT_VAR");
        GET_FROM_STACK(guid, "IMPORT_VAR");
        auto it = external_variables_.find(guid);
        if (it == external_variables_.end()) {
          error_ = StringPrintf("Unknown variable GUID %d at IMPORT_VAR", guid);
          return false;
        }
        VMVariableReference ref;
        ref.var = it->second.get();
        ref.arg = arg;
        variable_stack_.emplace_back(std::move(ref));
        operand_stack_.push_back(variable_stack_.size() - 1);
        break;
      }
      case CREATE_INDIRECT_VAR: {
        GET_VARINT(fpofs);
        VMVariableReference ref;
        ref.var = &operand_stack_variables_;
        ref.arg = fp_ + fpofs;
        variable_stack_.emplace_back(std::move(ref));
        operand_stack_.push_back(variable_stack_.size() - 1);
        break;
      }
      case CREATE_STATIC_VAR: {
        GET_VARINT(guid);
        bool created = false;
        auto& global_ref = external_variables_[guid];
        if (!global_ref.get()) {
          created = true;
          global_ref.reset(new StaticVariable);
        }
        VMVariableReference ref;
        ref.var = global_ref.get();
        ref.arg = 0;
        variable_stack_.emplace_back(std::move(ref));
        operand_stack_.push_back(variable_stack_.size() - 1);
        operand_stack_.push_back(created ? 1 : 0);
        break;
      }
      case NUMERIC_PLUS: {
        GET_FROM_STACK(rhs, "NUMERIC_PLUS");
        GET_FROM_STACK(lhs, "NUMERIC_PLUS");
        operand_stack_.push_back(lhs + rhs);
        break;
      }
      case NUMERIC_MINUS: {
        GET_FROM_STACK(rhs, "");
        GET_FROM_STACK(lhs, "");
        operand_stack_.push_back(lhs - rhs);
        break;
      }
      case NUMERIC_MUL: {
        GET_FROM_STACK(rhs, "");
        GET_FROM_STACK(lhs, "");
        operand_stack_.push_back(lhs * rhs);
        break;
      }
      case NUMERIC_DIV: {
        GET_FROM_STACK(rhs, "");
        GET_FROM_STACK(lhs, "");
        if (rhs == 0) {
          error_ = StringPrintf("Div by zero");
          return false;
        }
        operand_stack_.push_back(lhs / rhs);
        break;
      }
      case NUMERIC_MOD: {
        GET_FROM_STACK(rhs, "");
        GET_FROM_STACK(lhs, "");
        if (rhs == 0) {
          error_ = StringPrintf("Div by zero");
          return false;
        }
        operand_stack_.push_back(lhs % rhs);
        break;
      }
      case BOOL_EQ: {
        GET_FROM_STACK(rhs, "");
        GET_FROM_STACK(lhs, "");
        rhs = !!rhs;
        lhs = !!lhs;
        operand_stack_.push_back(lhs == rhs);
        break;
      }
      case BOOL_NOT: {
        GET_FROM_STACK(rhs, "");
        if (rhs == 0) {
          operand_stack_.push_back(1);
        } else {
          operand_stack_.push_back(0);
        }
        break;
      }
      case BOOL_PROJECT: {
        GET_FROM_STACK(rhs, "");
        if (rhs == 0) {
          operand_stack_.push_back(0);
        } else {
          operand_stack_.push_back(1);
        }
        break;
      }
      case JUMP: {
        GET_VARINT(r);
        jump(get_ip() + r);
        break;
      }
      case CALL: {
        if (call_stack_.size() < 1) {
          error_ = StringPrintf("Call stack underflow at CALL");
          return false;
        }
        if (fp_ != call_stack_.back().fp) {
          error_ = StringPrintf("Unexpected fp at CALL. Expected %d actual %d",
                                call_stack_.back().fp, fp_);
          return false;
        }
        GET_FROM_STACK(dst, "");
        GET_VARINT(num_arg);
        call_stack_.emplace_back();
        call_stack_.back().return_address = get_ip();
        call_stack_.back().fp = operand_stack_.size() - num_arg;
        fp_ = call_stack_.back().fp;
        call_stack_.back().vp = variable_stack_.size();
        jump(dst);
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
        jump(s.return_address);
        call_stack_.pop_back();
        const auto& ss = call_stack_.back();
        fp_ = ss.fp;
        break;
      }
      case TEST_JUMP_IF_FALSE: {
        GET_VARINT(r);
        GET_FROM_STACK(arg, "TEST_JUMP_IF_FALSE");
        if (arg == 0) {
          _ip_ += r;
        }
        break;
      }
      case TEST_JUMP_IF_TRUE: {
        GET_VARINT(r);
        GET_FROM_STACK(arg, "TEST_JUMP_IF_TRUE");
        if (arg != 0) {
          _ip_ += r;
        }
        break;
      }
      case PRINT_NUM: {
        GET_FROM_STACK(arg, "PRINT_NUM");
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
        error_ = StringPrintf("Unexpected instruction %02x", insn);
        return false;
    }
  };
  return true;
};

} // namespace logic
