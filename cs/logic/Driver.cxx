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
 * \file Driver.cxx
 *
 * Controls the parsing and compiling execution.
 *
 * @author Balazs Racz
 * @date 15 June 2019
 */

#include "logic/Driver.hxx"
#include "logic/Parser.hxxout"

namespace logic {

int Driver::parse_file(const std::string& f) {
  filename_ = f;
  scan_begin();
  //parser.set_debug_level(debug_level_ & TRACE_PARSE ? 1 : 0);
  int res = parser.parse();
  scan_end();
  if (!error_output_.empty()) return 1;
  return res;
}

void Driver::serialize(std::string* output) {
  output_root_ = output;
  string preamble;
  // Renders preamble.
  for (const auto& c : commands_) {
    c->serialize_preamble(&preamble);
    /// @todo we need to somehow syntactically prevent users from declaring
    /// exported variables in a context that is not okay for that.
  }
  BytecodeStream::append_opcode(&preamble, TERMINATE);
  BytecodeStream::append_opcode(output, IF_PREAMBLE);
  BytecodeStream::append_opcode(output, TEST_JUMP_IF_FALSE);
  BytecodeStream::append_varint(output, preamble.size());
  output->append(preamble);
  
  // Allocated global variables.  We do not need to reserve space for these on
  // the stack, because they will get pushed when we pass the declaration
  // instruction.
  //
  // BytecodeStream::append_opcode(output, ENTER);
  // BytecodeStream::append_varint(output, current_context()->frame_size_);

  // Renders instructions.
  for (const auto& c : commands_) {
    c->serialize(output);
  }
}

void Driver::error(const yy::location& l, const std::string& m) {
  string txt;
  //if (l.begin.filename) {
  //  txt.append(*l.begin.filename);
  //  txt.push_back(':');
  //}
  txt.append(StringPrintf("%d.%d", l.begin.line, l.begin.column));
  if (l.end.line > l.begin.line) {
    txt.append(StringPrintf("-%d.%d", l.end.line, l.end.column));
  } else if (l.end.column > l.begin.column) {
    txt.append(StringPrintf("-%d", l.end.column));
  }
  txt.append(": ");
  error_output_.append(txt);
  error_output_.append(m);
  error_output_.push_back('\n');
}

void Driver::error(const std::string& m) {
  error_output_.append(m);
  error_output_.push_back('\n');
}

void Function::register_function(std::string* output) {
  if (!driver_->is_output_root(output)) {
    driver_->error(StringPrintf("Function %s not defined at the top level.",
                                name_.c_str()));
    return;
  }
  unsigned jump_ofs = output->size();
  auto* s = driver_->find_mutable_symbol(name_);
  if (!s) {
    driver_->error(StringPrintf(
        "Function %s: symbol table entry not found during serialize.",
        name_.c_str()));
    return;
  }
  s->fp_offset_ = jump_ofs;
}


} // namespace logic
