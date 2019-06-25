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
 * \file Driver.hxx
 *
 * Controls the parsing and compiling execution.
 *
 * @author Balazs Racz
 * @date 15 June 2019
 */

#ifndef _LOGIC_DRIVER_HXX_
#define _LOGIC_DRIVER_HXX_
#include <map>
#include <memory>
#include <string>

#include "logic/AST.hxx"
#include "logic/Parser.hxxout"

namespace logic {
logic::yy::Parser::symbol_type yylex(logic::Driver& driver);
}

// Declaration of the lexer function's prototype.
#define YY_DECL \
  logic::yy::Parser::symbol_type logic::yylex(logic::Driver& driver)
// This acutally invokes that declaration.
YY_DECL;

namespace logic {

// Encapsulation of the scanning and parsing process.
class Driver {
 public:
  /// Constructor.
  Driver();

  struct ParsingContext {
    /// Symbol table available in the current context.
    std::map<std::string, Symbol> symbol_table_;

    /// How many variables to allocate on the operand stack when entering this
    /// context.
    unsigned frame_size_{0};
  };

  ParsingContext current_context_;

  /// @return the current syntactical context.
  ParsingContext* current_context() {
    return &current_context_;
  }

  /// @return true if the currently parsing context is the global variable
  /// context.
  bool is_global_context() {
    return true; /// @todo implement other contexts.
  }
  
  /// Create a new local variable.
  /// @param name is the identifier ofthe local variable.
  /// @param type is the type (shall be LOCAL_VAR_NN).
  /// @return true if the allocation was successful.
  bool allocate_variable(const string& name, Symbol::Type type) {
    if (current_context()->symbol_table_.find(name) !=
        current_context()->symbol_table_.end()) {
      string err = "Identifier '";
      err += name;
      err += "' is already declared.";
      error(err);
      return false;
    }
    auto& s = current_context()->symbol_table_[name];
    s.symbol_type_ = type;
    s.fp_offset_ = current_context()->frame_size_;
    current_context()->frame_size_++;
    return true;
  }

  /// Looks up a symbol in the symbol table.
  /// @param name is the identifier of the symbol.
  /// @return symbol table entry, or null if this is an undeclared symbol.
  const Symbol* find_symbol(const string& name) {
    auto it = current_context()->symbol_table_.find(name);
    if (it == current_context()->symbol_table_.end()) {
      return nullptr;
    }
    return &it->second;
  }

  /// Finds a variable type symbol in the symbol table. Reports an error and
  /// returns nullptr if the symbol is not found or not of the expected type.
  /// @param name is the symbol to look up.
  /// @param los is the location where the symbol was found; used for error printouts.
  /// @param expected_type describes what type of variable we are looking for.
  /// @return null upon error, or the symbol table entry.
  const Symbol* get_variable(const string& name, const yy::location& loc, Symbol::Type expected_type) {
    const Symbol* s = find_symbol(name);
    if (!s) {
      string err = "Undeclared variable '";
      err += name;
      err += "'";
      error(loc, err);
      return nullptr;
    } else if (s->symbol_type_ != expected_type) {
      string err = "'";
      err += name;
      err += "' incorrect type; expected ";
      switch (expected_type) {
        case Symbol::LOCAL_VAR_BOOL:
          err += "bool";
          break;
        case Symbol::LOCAL_VAR_INT:
          err += "int";
          break;
        default:
          err += "?\?\?";
      }
      error(loc, err);
      return nullptr;
    }
    return s;
  }

  /// The parsed AST of global statements.
  std::vector<std::shared_ptr<Command> > commands_;

  /// @return a debug printout of the entire AST after parsing.
  std::string debug_print() {
    std::string ret;
    for (const auto& c : commands_) {
      c->debug_print(&ret);
      ret.append(";\n");
    }
    return ret;
  }

  /// Serializes all commands into bytecode after the parsing.
  /// @param output it the container where the compiled bytecode will go.
  void serialize(std::string* output) {
    // Allocated global variables
    BytecodeStream::append_opcode(output, ENTER);
    BytecodeStream::append_varint(output, current_context()->frame_size_);
    // Renders instructions.
    for (const auto& c : commands_) {
      c->serialize(output);
    }
  }

  /// lexical context variable that describes what storage option the current
  /// variable declaration has.
  VariableStorageSpecifier decl_storage_;

  /// True if we are in the global scope.
  // not currently used yet.
  //bool is_global_scope_;
  
  int result;
  // Run the parser on file F.
  // Return 0 on success.
  int parse_file(const std::string& filename);
  // Error handling.
  void error(const yy::location& l, const std::string& m);
  void error(const std::string& m);

  /// @todo make this const string*
  std::string* get_filename() {
    return &filename_;
  }

  /// Alocates a new GUID for an exported variable.
  int allocate_guid() { return next_guid_++; }

 private:
  /// The name of the file being parsed.
  /// Used later to pass the file name to the location tracker.
  std::string filename_;

  // Handling the scanner.
  void scan_begin();
  void scan_end();

  enum DebugLevel {
    NO_TRACE,
    TRACE_PARSE = 1,
    TRACE_LEX = 2,
    TRACE_LEX_AND_PARSE = 3,
  } debug_level_{NO_TRACE};

  /// Next GUID to assign to a variable.
  int next_guid_{1};
};

} // namespace logic

#endif  // _LOGIC_DRIVER_HXX_
