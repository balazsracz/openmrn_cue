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
  COMMA ","
  ASSIGN  "="
  MINUS   "-"
  PLUS    "+"
  STAR    "*"
  SLASH   "/"
  PERCENT "%"
  BANG    "!"
  LPAREN  "("
  RPAREN  ")"
  LBRACE  "{"
  RBRACE  "}"
  DOUBLEAND  "&&"
  DOUBLEOR  "||"
  DOUBLEEQ  "=="
  IF  "if"
  ELSE  "else"
  TYPEINT  "int"
  TYPEBOOL  "bool"
  EXPORTED  "exported"
  AUTO  "auto"
;
%token <std::string> IDENTIFIER "identifier"
%token <std::string> STRING "string"
%token <int> NUMBER "number"
%token <bool> BOOL "constbool"
%type  <logic::VariableStorageSpecifier> storage_specifier
%type  <std::shared_ptr<logic::IntExpression> > exp
%type  <std::shared_ptr<logic::IntExpression> > decl_optional_int_exp
%type  <std::shared_ptr<logic::BooleanExpression> > boolexp
%type  <std::shared_ptr<logic::BooleanExpression> > decl_optional_bool_exp
%type  <std::shared_ptr<logic::Command> > command
%type  <std::shared_ptr<logic::Command> > assignment
%type  <std::shared_ptr<logic::Command> > variable_decl
%type  <std::shared_ptr<logic::Command> > int_decl_single
%type  <std::shared_ptr<logic::Command> > bool_decl_single
%type  <std::shared_ptr<std::vector<std::shared_ptr<logic::Command> > > > commands
%type  <std::shared_ptr<std::vector<std::shared_ptr<logic::Command> > > > int_decl_list
%type  <std::shared_ptr<std::vector<std::shared_ptr<logic::Command> > > > bool_decl_list
//%printer { yyoutput << $$; } <*>;
%%
%start unit;
unit: commands { driver.commands_.swap(*$1); };

assignment:
"identifier" "=" "identifier" {
  // Polymorphic assignment.
  const Symbol* ls = driver.find_symbol($1);
  Symbol::DataType dt = Symbol::DATATYPE_INT;
  if (ls) {
    dt = ls->get_data_type();
  }
  auto vp =
      driver.get_variable_reference(std::move($1), @1, dt);
  auto rp =
      driver.get_variable_reference(std::move($3), @3, dt);
  if (!vp || !rp) {
    YYERROR;
  }
  switch(dt) {
    case Symbol::DATATYPE_INT:
      $$ = std::make_shared<NumericAssignment>(
          std::move(vp), std::make_shared<IntVariable>(std::move(rp)));
      break;
    case Symbol::DATATYPE_BOOL:
      $$ = std::make_shared<BooleanAssignment>(
          std::move(vp), std::make_shared<BoolVariable>(std::move(rp)));
      break;
    default:
      driver.error(@1, "Unexpected data type");
      YYERROR;
  }
} |
"identifier" "=" exp {
  auto vp =
      driver.get_variable_reference(std::move($1), @1, Symbol::DATATYPE_INT);
  if (!vp) {
    YYERROR;
  }
  $$ = std::make_shared<NumericAssignment>(std::move(vp), std::move($3));
}
| "identifier" "=" boolexp {
  auto vp =
      driver.get_variable_reference(std::move($1), @1, Symbol::DATATYPE_BOOL);
  if (!vp) {
    YYERROR;
  }
  $$ = std::make_shared<BooleanAssignment>(std::move(vp), std::move($3));
}
;

decl_optional_int_exp:
%empty { $$.reset(); }
| "=" exp { $$ = std::move($2); };

int_decl_single:
"identifier" decl_optional_int_exp {
  Symbol::Type t = Symbol::LOCAL_VAR_INT;
  if (driver.decl_storage_ == INDIRECT_VAR) {
    t = Symbol::INDIRECT_VAR_INT;
  }
  if (!driver.allocate_variable($1, t)) {
    YYERROR;
  }
  if (driver.decl_storage_ == INDIRECT_VAR) {
    if ($2) {
      driver.error(
          @2, "Exported variable declaration cannot have an initializer.");
      YYERROR;
    }
    $$ = std::make_shared<IndirectVarCreate>($1, driver.find_symbol($1)->fp_offset_, driver.allocate_guid());
  } else {
    // Local var.
    auto vp =
        driver.get_variable_reference(std::move($1), @1, Symbol::DATATYPE_INT);
    if (!vp) YYERROR;
    $$ = std::make_shared<NumericAssignment>(
        std::move(vp), $2 ? std::move($2) : std::make_shared<IntConstant>(0));
  }
}

int_decl_list:
int_decl_single {
  $$ = std::make_shared<std::vector<std::shared_ptr<logic::Command> > >();
  $$->emplace_back(std::move($1));
} |
int_decl_list "," int_decl_single {
  $$.swap($1);
  $$->emplace_back(std::move($3));
}
;


decl_optional_bool_exp:
%empty { $$.reset(); }
| "=" boolexp { $$ = std::move($2); };

bool_decl_single:
"identifier" decl_optional_bool_exp {
  Symbol::Type t = Symbol::LOCAL_VAR_BOOL;
  if (driver.decl_storage_ == INDIRECT_VAR) {
    t = Symbol::INDIRECT_VAR_BOOL;
  }
  if (!driver.allocate_variable($1, t)) {
    YYERROR;
  }
  if (driver.decl_storage_ == INDIRECT_VAR) {
    if ($2) {
      driver.error(
          @2, "Exported variable declaration cannot have an initializer.");
      YYERROR;
    }
    $$ = std::make_shared<IndirectVarCreate>($1, driver.find_symbol($1)->fp_offset_, driver.allocate_guid());
  } else {
    // Local var.
    auto vp =
        driver.get_variable_reference(std::move($1), @1, Symbol::DATATYPE_BOOL);
    if (!vp) YYERROR;
    $$ = std::make_shared<BooleanAssignment>(
        std::move(vp),
        $2 ? std::move($2) : std::make_shared<BooleanConstant>(false));
  }
}

bool_decl_list:
bool_decl_single {
  $$ = std::make_shared<std::vector<std::shared_ptr<logic::Command> > >();
  $$->emplace_back(std::move($1));
} |
bool_decl_list "," bool_decl_single {
  $$.swap($1);
  $$->emplace_back(std::move($3));
}
;

storage_specifier:
%empty { $$ = LOCAL_VAR; }
| "exported" { $$ = INDIRECT_VAR; }
;


optional_semicolon:
%empty {}
| ";"
;

variable_decl:
storage_specifier "int" {
  driver.decl_storage_ = $1;
  if ($1 == INDIRECT_VAR && !driver.is_global_context()) {
    driver.error(@1, "Exported variables must appear in the global context.");
  }
} int_decl_list {
  if ($4->size() == 1) {
    std::swap($$, (*$4)[0]);
  } else {
    $$ = std::make_shared<CommandSequence>(std::move(*$4));  
  }
} |
storage_specifier "bool" { driver.decl_storage_ = $1; } bool_decl_list {
  if ($4->size() == 1) {
    std::swap($$, (*$4)[0]);
  } else {
    $$ = std::make_shared<CommandSequence>(std::move(*$4));  
  }
}
;

command:
assignment optional_semicolon { $$ = std::move($1); };
| "{" commands "}" { $$ = std::make_shared<CommandSequence>(std::move(*$2)); }
| "if" "(" boolexp ")" "{" commands "}" "else" "{" commands "}" {
  $$ = std::make_shared<IfThenElse>(
      std::move($3),
      std::make_shared<CommandSequence>(std::move(*$6)),
      std::make_shared<CommandSequence>(std::move(*$10))); }
| "if" "(" boolexp ")" "{" commands "}" {
  $$ = std::make_shared<IfThenElse>(
      std::move($3),
      std::make_shared<CommandSequence>(std::move(*$6)));
  }
| variable_decl optional_semicolon { $$ = std::move($1); }
;



commands:
%empty { $$ = std::make_shared<std::vector<std::shared_ptr<Command>>>(); }
| commands command {
  $1->push_back(std::move($2));
  $$ = std::move($1);
};

%left "+" "-";
%left "*" "/" "%";
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
| "identifier"  {
  auto vp =
      driver.get_variable_reference(std::move($1), @1, Symbol::DATATYPE_INT);
  if (!vp) {
    YYERROR;
  }
  $$ = std::make_shared<IntVariable>(std::move(vp));
}
;

%left "==" "!=";
%left "&&";
%left "||";

boolexp:
  "constbool"      { $$ = std::make_shared<BooleanConstant>($1); }
|  boolexp "==" boolexp   { $$ = std::make_shared<BoolEq>(std::move($1), std::move($3)); }
| "(" boolexp ")"   { std::swap ($$, $2); }
| "!" boolexp       { $$ = std::make_shared<BoolNot>(std::move($2)); }
|  boolexp "&&" boolexp   { $$ = std::make_shared<BoolAnd>(std::move($1), std::move($3)); }
|  boolexp "||" boolexp   { $$ = std::make_shared<BoolOr>(std::move($1), std::move($3)); }
| "identifier"  {
  auto vp =
      driver.get_variable_reference(std::move($1), @1, Symbol::DATATYPE_BOOL);
  if (!vp) {
    YYERROR;
  }
  $$ = std::make_shared<BoolVariable>(std::move(vp));
}
;

%%

void logic::yy::Parser::error (const location_type& l, const std::string& m)
{
  driver.error (l, m);
}
