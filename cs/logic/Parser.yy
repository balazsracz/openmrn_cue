/* -*- C++ -*- */
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
 * \file Parser.yy
 *
 * Bison command file for the logic language parser.
 *
 * @author Balazs Racz
 * @date 15 June 2019
 */

%skeleton "lalr1.cc" 
%require ""
%defines
%define parser_class_name {Parser}
%define api.namespace {logic::yy}
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%code requires
{
#include <memory>
#include <string>
#include "logic/AST.hxx"
namespace logic {
class Driver;
}
}
// The parsing context.
%param { logic::Driver& driver }

%locations
%initial-action
{
  // Initialize the initial location.
  @$.begin.filename = @$.end.filename = driver.get_filename();
};
//%define parse.trace
%define parse.error verbose
%code
{
#include "logic/Driver.hxx"
}
%define api.token.prefix {TOK_}
%token
  END  0  "end of file"
  SEMICOLON  ";"
  COMMA   ","
  ASSIGN  "="
  MINUS   "-"
  PLUS    "+"
  STAR    "*"
  SLASH   "/"
  PERCENT "%"
  BANG    "!"
  AMP     "&"
  LPAREN  "("
  RPAREN  ")"
  RBRACE  "}"
  DOUBLEAND "&&"
  DOUBLEOR  "||"
  DOUBLEEQ  "=="
  NEQ     "!="
  LEQ     "<="
  GEQ     ">="
  LT      "<"
  GT      ">"
  IF      "if"
  ELSE    "else"
  TYPEINT "int"
  TYPEBOOL "bool"
  TYPEVOID "void"
  EXPORTED "exported"
  MUTABLE  "mutable"
  MAX_STATE "max_state"
  DESCRIPTION "description"
  AUTO    "auto"
  STATIC  "static"
  PRINT  "print"
  TERMINATE  "terminate"
;
%token <int> LBRACE "{" // stores the fp_offset_ to return to.
%token <std::string> UNDECL_ID "undeclared_identifier"
%token <std::string> BOOL_VAR_ID "bool_var_identifier"
%token <std::string> INT_VAR_ID "int_var_identifier"
%token <std::string> VOID_FN_ID "void_function_identifier"
%token <std::string> INT_FN_ID "int_function_identifier"
%token <std::string> BOOL_FN_ID "bool_function_identifier"
%token <std::string> STRING "string"
%token <int> NUMBER "number"
%token <bool> BOOL "constbool"
%type  <std::string> anyfnname
%type  <logic::Symbol::Access> storage_specifier
%type  <logic::Symbol::Access> fn_arg_storage_specifier
%type  <logic::TypeSpecifier> type_specifier
%type  <std::shared_ptr<logic::IntExpression> > exp
%type  <std::shared_ptr<logic::BooleanExpression> > boolexp
%type  <std::shared_ptr<logic::Command> > command
%type  <std::shared_ptr<logic::Command> > braced_commands
%type  <std::shared_ptr<logic::Command> > conditional
%type  <std::shared_ptr<logic::Command> > assignment
%type  <std::shared_ptr<logic::Command> > fncall
%type  <std::shared_ptr<std::vector<std::shared_ptr<logic::PolymorphicExpression> > > > fncallargs
%type  <std::shared_ptr<logic::PolymorphicExpression> > poly_expression
%type  <std::shared_ptr<logic::Command> > print
%type  <std::shared_ptr<std::vector<std::shared_ptr<logic::Command> > > > printargs
%type <std::shared_ptr<logic::Command> > printonearg
%type  <std::shared_ptr<logic::PolymorphicExpression> > optional_assignment_expression
%type  <std::shared_ptr<logic::Command> > variable_decl
%type  <std::shared_ptr<std::vector<std::shared_ptr<logic::Command> > > > commands
%type  <std::shared_ptr<Function> > function
%type  <std::shared_ptr<FunctionArgument> > function_arg
%type  <std::shared_ptr<std::vector<std::shared_ptr<FunctionArgument> > > > function_arg_list
//%printer { yyoutput << $$; } <*>;
%%
%start unit;
unit: commands { driver.commands_.swap(*$1); };

%left "||";
%left "&&";
%left "==" "!=";
%nonassoc "<" "<=" ">" ">=";
%left "+" "-";
%left "*" "/" "%";
%right "!";

assignment:
"int_var_identifier" "=" exp {
  auto vp =
      driver.get_variable_reference(std::move($1), @1, Symbol::DATATYPE_INT);
  if (!vp) {
    YYERROR;
  }
  $$ = std::make_shared<NumericAssignment>(std::move(vp), std::move($3));
}
| "bool_var_identifier" "=" boolexp {
  auto vp =
      driver.get_variable_reference(std::move($1), @1, Symbol::DATATYPE_BOOL);
  if (!vp) {
    YYERROR;
  }
  $$ = std::make_shared<BooleanAssignment>(std::move(vp), std::move($3));
}
;

storage_specifier:
%empty { $$ = Symbol::LOCAL_VAR; }
| "exported" { $$ = Symbol::INDIRECT_VAR; }
| "static" { $$ = Symbol::STATIC_VAR; }
;

fn_arg_storage_specifier:
%empty { $$ = Symbol::LOCAL_VAR; }
| "mutable" { $$ = Symbol::INDIRECT_VAR; }
;


optional_semicolon:
%empty {}
|
";"
;

optional_assignment_expression:
%empty { $$ = std::make_shared<PolymorphicExpression>(); }
| "=" exp {
  $$ = std::make_shared<PolymorphicExpression>(std::move($2));
  $$->loc_ = @2;
}
| "=" boolexp {
  $$ = std::make_shared<PolymorphicExpression>(std::move($2));
  $$->loc_ = @2;
};

variable_parameters:
%empty {} |
variable_parameters "max_state" "(" "number" ")" {
  /// @todo
} |
variable_parameters "description" "(" "string" ")" {}
;

multi_variable_decl:
%empty {} |
multi_variable_decl "," "undeclared_identifier" variable_parameters optional_assignment_expression {
  auto cmd = driver.declare_variable(std::move($3), @3, $5.get(), @5);
  if (!cmd) {
    YYERROR;
  }
  driver.decl_helper_.emplace_back(std::move(cmd));
};

variable_decl:
storage_specifier type_specifier "undeclared_identifier" optional_assignment_expression {
  if ($1 != Symbol::LOCAL_VAR && !driver.is_global_context()) {
    driver.error(@1, "Exported and static variables must appear in the global context.");
    YYERROR;
  }
  driver.decl_storage_ = $1;
  driver.decl_type_ = $2;
  driver.decl_helper_.clear();
  auto cmd = driver.declare_variable(std::move($3), @3, $4.get(), @4);
  if (!cmd) {
    YYERROR;
  }
  driver.decl_helper_.emplace_back(std::move(cmd));
} multi_variable_decl optional_semicolon {
  if (driver.decl_helper_.size() > 1) {
    $$ = std::make_shared<CommandSequence>(std::move(driver.decl_helper_));
  } else {
    $$ = std::move(driver.decl_helper_[0]);
  }
  driver.decl_helper_.clear();
};

braced_commands:
"{" {
  if ($1 >= 0) {
    driver.error(@1, "Internal error: unexpected positive brace contents.");
    YYERROR;
  }
  $1 = driver.current_context()->frame_size_;
} commands "}" {
  if ($1 < 0) {
    driver.error(@1, "Internal error: unexpected negative brace contents.");
    YYERROR;
  }
  if ((int)driver.current_context()->frame_size_ != $1) {
    // we have local variables.
    $$ = std::make_shared<CommandSequence>(
        std::move(*$3), driver.current_context()->frame_size_, $1);
    driver.trim_stack($1);
    driver.current_context()->frame_size_ = $1;
  } else {
    $$ = std::make_shared<CommandSequence>(
        std::move(*$3), driver.current_context()->frame_size_);
  }
};

conditional:
 "if" "(" boolexp ")" braced_commands "else" braced_commands {
  $$ =
      std::make_shared<IfThenElse>(std::move($3), std::move($5), std::move($7));
}
| "if" "(" boolexp ")" braced_commands "else" conditional {
  $$ =
      std::make_shared<IfThenElse>(std::move($3), std::move($5), std::move($7));
}
| "if" "(" boolexp ")" braced_commands {
  $$ = std::make_shared<IfThenElse>(std::move($3), std::move($5));
}
;

poly_expression:
exp {
  $$ = std::make_shared<PolymorphicExpression>(std::move($1));
  $$->loc_ = @1;
} |
boolexp {
  $$ = std::make_shared<PolymorphicExpression>(std::move($1));
  $$->loc_ = @1;
} |
"&" "int_var_identifier" {
  $$ = std::make_shared<PolymorphicExpression>(Symbol::DATATYPE_INT,
    driver.get_variable_reference(std::move($2), @2, Symbol::DATATYPE_INT));
  $$->loc_ = @2;
} |
"&" "bool_var_identifier" {
  $$ = std::make_shared<PolymorphicExpression>(Symbol::DATATYPE_BOOL,
    driver.get_variable_reference(std::move($2), @2, Symbol::DATATYPE_BOOL));
  $$->loc_ = @2;
};

fncallargs:
%empty {
  $$ = std::make_shared<std::vector<std::shared_ptr<logic::PolymorphicExpression> > >();
} |
poly_expression {
  $$ = std::make_shared<std::vector<std::shared_ptr<logic::PolymorphicExpression> > >();
  $$->emplace_back(std::move($1));
} | 
fncallargs "," poly_expression {
  $$ = std::move($1);
  $$->emplace_back(std::move($3));
};

anyfnname:
"int_function_identifier" { $$ = std::move($1); }
| "bool_function_identifier" { $$ = std::move($1); }
| "void_function_identifier" { $$ = std::move($1); }
;

fncall:
anyfnname "(" {
} fncallargs ")" optional_semicolon {
  $$ = driver.get_function_call<FunctionCall>(std::move($1), @$, std::move($4),
                                              false);
  if (!$$) {
    YYERROR;
  }
};

printonearg:
"string" {
  $$ = std::make_shared<PrintString>(std::move($1));
} |
exp {
  $$ = std::make_shared<PrintInt>(std::move($1));
} /* |
boolexp {
  $$ = std::make_shared<PrintBool>(std::move($1));
}*/
;

printargs:
printonearg {
  $$ = std::make_shared<std::vector<std::shared_ptr<Command>>>();
  $$->push_back(std::move($1));
} |
printargs "," printonearg {
  $$ = std::move($1);
  $$->push_back(std::move($3));
};

print:
"print" "(" printargs ")" optional_semicolon {
  if ($3->size() == 1) {
    $$ = std::move((*$3)[0]);
  } else {
    $$ = std::make_shared<CommandSequence>(std::move(*$3));
  }
};

command:
assignment optional_semicolon { $$ = std::move($1); };
| braced_commands { $$ = std::move($1); }
| conditional { $$ = std::move($1); } 
| variable_decl optional_semicolon { $$ = std::move($1); }
| function { $$ = std::move($1); }
| fncall { $$ = std::move($1); }
| print { $$ = std::move($1); }
| "terminate" "(" ")" optional_semicolon {
  $$ = std::make_shared<Terminate>();
}
;

// @todo we need to switch the following to natively create CommandSequence and
// do away with this vector business. The CommandSequence should have an Add()
// method.

commands:
%empty { $$ = std::make_shared<std::vector<std::shared_ptr<Command>>>(); }
| commands command {
  $1->push_back(std::move($2));
  $$ = std::move($1);
};

exp:
exp "+" exp   {
  $$ = std::make_shared<IntBinaryExpression>(NUMERIC_PLUS, std::move($1),
                                             std::move($3));
} |
exp "-" exp   {
  $$ = std::make_shared<IntBinaryExpression>(NUMERIC_MINUS, std::move($1),
                                             std::move($3));
} |
exp "*" exp   {
  $$ = std::make_shared<IntBinaryExpression>(NUMERIC_MUL, std::move($1),
                                             std::move($3));
} |
exp "/" exp   {
  $$ = std::make_shared<IntBinaryExpression>(NUMERIC_DIV, std::move($1),
                                             std::move($3));
} |
exp "%" exp   {
  $$ = std::make_shared<IntBinaryExpression>(NUMERIC_MOD, std::move($1),
                                             std::move($3));
} 
| "(" exp ")"   { std::swap ($$, $2); }
| "number"      { $$.reset(new IntConstant($1)); }
| "-" "number"      { $$.reset(new IntConstant(-$2)); }
| "int_var_identifier"  {
  auto vp =
      driver.get_variable_reference(std::move($1), @1, Symbol::DATATYPE_INT);
  if (!vp) {
    YYERROR;
  }
  $$ = std::make_shared<IntVariable>(std::move(vp));
}
| "int_function_identifier" "(" fncallargs ")" {
  auto* s = driver.find_function($1);
  if (!s) {
    YYERROR;
  }
  $$ = std::make_shared<IntFunctionCall>(
      driver.current_context()->frame_size_, $1, *s, std::move($3), true);
}
;

boolexp:
  "constbool"      { $$ = std::make_shared<BooleanConstant>($1); }
|  boolexp "==" boolexp   { $$ = std::make_shared<BoolCmp>(BOOL_EQ, std::move($1), std::move($3)); }
|  boolexp "!=" boolexp   { $$ = std::make_shared<BoolCmp>(BOOL_NEQ, std::move($1), std::move($3)); }
| "(" boolexp ")"   { std::swap ($$, $2); }
| "!" boolexp       { $$ = std::make_shared<BoolNot>(std::move($2)); }
|  boolexp "&&" boolexp   { $$ = std::make_shared<BoolAnd>(std::move($1), std::move($3)); }
|  boolexp "||" boolexp   { $$ = std::make_shared<BoolOr>(std::move($1), std::move($3)); }
|  exp "<=" exp { $$ = std::make_shared<IntComp>(NUMERIC_LEQ, std::move($1), std::move($3));  }
|  exp ">=" exp { $$ = std::make_shared<IntComp>(NUMERIC_GEQ, std::move($1), std::move($3));  }
|  exp "<" exp { $$ = std::make_shared<IntComp>(NUMERIC_LT, std::move($1), std::move($3));  }
|  exp ">" exp { $$ = std::make_shared<IntComp>(NUMERIC_GT, std::move($1), std::move($3));  }
|  exp "==" exp { $$ = std::make_shared<IntComp>(NUMERIC_EQ, std::move($1), std::move($3));  }
|  exp "!=" exp { $$ = std::make_shared<IntComp>(NUMERIC_NEQ, std::move($1), std::move($3));  }
| "bool_var_identifier"  {
  auto vp =
      driver.get_variable_reference(std::move($1), @1, Symbol::DATATYPE_BOOL);
  if (!vp) {
    YYERROR;
  }
  $$ = std::make_shared<BoolVariable>(std::move(vp));
}
| "bool_function_identifier" "(" fncallargs ")" {
  auto* s = driver.find_function($1);
  if (!s) {
    YYERROR;
  }
  $$ = std::make_shared<BoolFunctionCall>(
      driver.current_context()->frame_size_, $1, *s, std::move($3), true);
}
;

type_specifier:
"int" {
  $$.builtin_type_ = Symbol::DATATYPE_INT;
} |
"bool" {
  $$.builtin_type_ = Symbol::DATATYPE_BOOL;
}  |
"void" {
  $$.builtin_type_ = Symbol::DATATYPE_VOID;
};


function:
storage_specifier type_specifier "undeclared_identifier" "(" {
  if (!driver.is_global_context()) {
    driver.error(
        @2, "Function definition is only allowed in the toplevel context.");
    YYERROR;
  }
  if ($1 != Symbol::LOCAL_VAR) {
    driver.error(
        @2, "Function definition cannot have exported storage specifier.");
    YYERROR;
  }
  auto* s = driver.allocate_symbol($3, @3, Symbol::FUNCTION);
  if (!s) {
    YYERROR;
  }
  s->data_type_ = $2.builtin_type_;
  driver.enter_function($2);
} function_arg_list ")" {
  auto* fsym = driver.find_mutable_symbol($3);
  if (!fsym) {
    driver.error(
        @3, "Internal error - symbol not found.");
    YYERROR;
  }
  fsym->args_ = $6;
  const auto& al = *$6;
  for (unsigned i = 0; i < al.size(); ++i) {
    auto* s = driver.find_symbol(al[i]->name_);
    if (!s) {
      driver.error(@6, StringPrintf("internal error: could not find argument %s", al[i]->name_.c_str()));
      YYERROR;
    }
    if (s->fp_offset_ != (int)i) {
      driver.error(@6, StringPrintf("internal error: for argument %s, expected fp offset %u, actual %d", al[i]->name_.c_str(), i, s->fp_offset_));
    }
  }
} "{" commands "}" {
  $$ = std::make_shared<Function>(&driver,
      std::move($3), std::move($2), std::move($6),
      std::make_shared<CommandSequence>(std::move(*$10)));
  driver.exit_function();
};

function_arg_list:
%empty {
  $$ = std::make_shared<std::vector<std::shared_ptr<FunctionArgument>>>();
} |
function_arg {
  $$ = std::make_shared<std::vector<std::shared_ptr<FunctionArgument>>>();
  $$->emplace_back(std::move($1));
} |
function_arg_list "," function_arg {
  $$ = std::move($1);
  $$->emplace_back(std::move($3));
};

function_arg:
fn_arg_storage_specifier type_specifier "undeclared_identifier" {
  auto* s = driver.allocate_variable($3, @3, Symbol::VARIABLE);
  s->access_ = $1;
  s->data_type_ = $2.builtin_type_;
  $$ = std::make_shared<FunctionArgument>(std::move($3), std::move($2), $1);
};

%%

void logic::yy::Parser::error (const location_type& l, const std::string& m)
{
  driver.error (l, m);
}
