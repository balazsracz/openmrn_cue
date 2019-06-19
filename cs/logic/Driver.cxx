#include "logic/Driver.hxx"
#include "logic/Parser.hxxout"

namespace logic {

Driver::Driver() {
}

int Driver::parse_file(const std::string& f) {
  filename_ = f;
  scan_begin();
  yy::Parser parser(*this);
  parser.set_debug_level(debug_level_ & TRACE_PARSE ? 1 : 0);
  int res = parser.parse();
  scan_end();
  return res;
}

void Driver::error(const yy::location& l, const std::string& m) {
  std::cerr << l << ": " << m << std::endl;
}

void Driver::error(const std::string& m) { std::cerr << m << std::endl; }

} // namespace logic
