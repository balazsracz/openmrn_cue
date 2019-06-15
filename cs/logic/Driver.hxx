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

  /// Symbol table of variables with values.
  std::map<std::string, int> variables;

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
    for (const auto& c : commands_) {
      c->serialize(output);
    }
  }

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
};

} // namespace logic

#endif  // _LOGIC_DRIVER_HXX_
