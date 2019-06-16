#ifndef _LOGIC_AST_HXX_
#define _LOGIC_AST_HXX_

#include <memory>

#include "utils/logging.h"
#include "utils/macros.h"
#include "utils/StringPrintf.hxx"

#include "logic/Bytecode.hxx"

namespace logic {

class Command {
 public:
  virtual ~Command() {}
  virtual void serialize(std::string* output) = 0;
  virtual void debug_print(std::string* output) = 0;
};

/// Compound command (aka brace enclosed command sequence).
class CommandSequence : public Command {
 public:
  CommandSequence(std::vector<std::shared_ptr<Command> > &&commands)
      : commands_(std::move(commands)) {}

  void serialize(std::string* output) override {
    for (const auto& c : commands_) {
      c->serialize(output);
    }
  }

  void debug_print(std::string* output) override {
    output->append("{\n");
    for (const auto& c : commands_) {
      c->debug_print(output);
      output->append("\n");
    }
    output->append("}\n");
  }

 private:
  std::vector<std::shared_ptr<Command> > commands_;
};

class NumericExpression : public Command {};

class NumericAssignment : public Command {
 public:
  NumericAssignment(std::string variable,
                    std::shared_ptr<NumericExpression> value)
      : variable_(std::move(variable)), value_(std::move(value)) {}

  void serialize(std::string* output) override {
    value_->serialize(output);
    /// @todo create representation of variables.
  }

  void debug_print(std::string* output) override {
    output->append("assign(");
    output->append(variable_);
    output->append(",");
    value_->debug_print(output);
    output->append(")");
  }

 private:
  std::string variable_;
  std::shared_ptr<NumericExpression> value_;
};

class NumericAdd : public NumericExpression {
 public:
  NumericAdd(std::shared_ptr<NumericExpression> left,
             std::shared_ptr<NumericExpression> right)
      : left_(std::move(left)), right_(std::move(right)) {
    HASSERT(left_);
    HASSERT(right_);
  }

  void serialize(std::string* output) override {
    left_->serialize(output);
    right_->serialize(output);
    BytecodeStream::append_opcode(output, NUMERIC_PLUS);
  }

  void debug_print(std::string* output) override {
    output->append("plus(");
    left_->debug_print(output);
    output->append(",");
    right_->debug_print(output);
    output->append(")");
  }

 private:
  std::shared_ptr<NumericExpression> left_;
  std::shared_ptr<NumericExpression> right_;
};

class NumericConstant : public NumericExpression {
 public:
  NumericConstant(int value) : value_(value) {}

  void serialize(std::string* output) override {
    if (value_ == 0) {
      BytecodeStream::append_opcode(output, PUSH_CONSTANT_0);
    } else if (value_ == 1) {
      BytecodeStream::append_opcode(output, PUSH_CONSTANT_1);
    } else {
      BytecodeStream::append_opcode(output, PUSH_CONSTANT);
      BytecodeStream::append_varint(output, value_);
    }
  }

  void debug_print(std::string* output) override {
    output->append(StringPrintf("%d", value_));
  }

 private:
  int value_;
};

class NumericVariable : public NumericExpression {
 public:
  NumericVariable(std::string variable) : variable_(std::move(variable)) {}

  void serialize(std::string* output) override {
    DIE("unimplemented");
  }

  void debug_print(std::string* output) override {
    output->append("fetch(");
    output->append(variable_);
    output->append(")");
  }

 private:
  std::string variable_;
};

class BooleanConstant : public Command {
 public:
  BooleanConstant(bool value) : value_(value) {}

  void serialize(std::string* output) override {
    BytecodeStream::append_opcode(output, value_ ? PUSH_CONSTANT_1 : PUSH_CONSTANT_0);
  }

  void debug_print(std::string* output) override {
    if (value_) {
      output->append("true");
    } else {
      output->append("false");
    }
  }

 private:
  bool value_;
};

} // namespace logic

#endif // _LOGIC_AST_HXX_
