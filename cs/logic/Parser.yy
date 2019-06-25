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
%define parse.trace
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
  LPAREN  "("
  RPAREN  ")"
  LBRACE  "{"
  RBRACE  "}"
  DOUBLEAND  "&&"
  DOUBLEOR  "||"
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
"identifier" "=" exp {
  const Symbol* s = driver.get_variable($1, @1, Symbol::LOCAL_VAR_INT);
  if (!s) {
    YYERROR;
  } else {
    $$.reset(new NumericAssignment($1, *s, std::move($3)));
  }
}
| "identifier" "=" boolexp {
  const Symbol* s = driver.get_variable($1, @1, Symbol::LOCAL_VAR_BOOL);
  if (!s) {
    YYERROR;
  } else {
    $$.reset(new BooleanAssignment($1, *s, std::move($3)));
  }
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
    
  } else {
    // Local var.
    $$ = std::make_shared<NumericAssignment>(
        $1, *driver.find_symbol($1),
        $2 ? std::move($2) : std::make_shared<IntConstant>(0));
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
%empty { $$ = std::make_shared<BooleanConstant>(false); }
| "=" boolexp { $$ = std::move($2); };

bool_decl_single:
"identifier" decl_optional_bool_exp {
  if (!driver.allocate_variable($1, Symbol::LOCAL_VAR_BOOL)) {
    YYERROR;
  }
  $$ = std::make_shared<BooleanAssignment>($1, *driver.find_symbol($1),
                                           std::move($2));
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

variable_decl:
storage_specifier "int" { driver.decl_storage_ = $1; } int_decl_list ";" {
  if ($4->size() == 1) {
    std::swap($$, (*$4)[0]);
  } else {
    $$ = std::make_shared<CommandSequence>(std::move(*$4));  
  }
} |
storage_specifier "bool" { driver.decl_storage_ = $1; } bool_decl_list ";" {
  if ($4->size() == 1) {
    std::swap($$, (*$4)[0]);
  } else {
    $$ = std::make_shared<CommandSequence>(std::move(*$4));  
  }
}
;

command:
assignment { $$ = std::move($1); };
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
| variable_decl { $$ = std::move($1); }
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
  const Symbol* s = driver.get_variable($1, @1, Symbol::LOCAL_VAR_INT);
  if (!s) {
    YYERROR;
  } else {
    $$ = std::make_shared<IntVariable>(std::move($1), *s);
  }
}
;

%left "==" "!=";
%left "&&";
%left "||";

boolexp:
  "constbool"      { $$ = std::make_shared<BooleanConstant>($1); }
| "(" boolexp ")"   { std::swap ($$, $2); }
|  boolexp "&&" boolexp   { $$ = std::make_shared<BoolAnd>(std::move($1), std::move($3)); }
|  boolexp "||" boolexp   { $$ = std::make_shared<BoolOr>(std::move($1), std::move($3)); }
| "identifier"  {
  const Symbol* s = driver.get_variable($1, @1, Symbol::LOCAL_VAR_BOOL);
  if (!s) {
    YYERROR;
  } else {
    $$ = std::make_shared<BoolVariable>(std::move($1), *s);
  }
}
;

%%

void logic::yy::Parser::error (const location_type& l, const std::string& m)
{
  driver.error (l, m);
}
