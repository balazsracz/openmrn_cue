%skeleton "lalr1.cc" /* -*- C++ -*- */
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
  ASSIGN  "="
  MINUS   "-"
  PLUS    "+"
  STAR    "*"
  SLASH   "/"
  PERCENT "%"
  LPAREN  "("
  RPAREN  ")"
;
%token <std::string> IDENTIFIER "identifier"
%token <int> NUMBER "number"
%token <bool> BOOL "bool"
%type  <std::shared_ptr<logic::NumericExpression> > exp
%type  <std::shared_ptr<logic::Command> > command
%type  <std::shared_ptr<logic::Command> > assignment
//%printer { yyoutput << $$; } <*>;
%%
%start unit;
unit: commands { };

assignment:
  "identifier" "=" exp { $$.reset(new NumericAssignment($1, std::move($3))); };

command:
  assignment { $$ = std::move($1); };

commands:
  %empty {}
| commands command { driver.commands_.push_back(std::move($2)); };

%left "+" "-";
%left "*" "/";
exp:
  exp "+" exp   { $$.reset(new NumericAdd(std::move($1), std::move($3))); }
| "(" exp ")"   { std::swap ($$, $2); }
| "number"      { $$.reset(new NumericConstant($1)); }
| "identifier"  { $$ = std::make_shared<NumericVariable>(std::move($1)); }
| "bool"      { $$ = std::make_shared<NumericConstant>($1 ? 1 : 0); };

//| exp "-" exp   { $$.reset(new NumericMinus(std::move($1), std::move($3))); }
//| exp "*" exp   { $$.reset(new NumericMul(std::move($1), std::move($3))); }
//| exp "/" exp   { $$.reset(new NumericDiv(std::move($1), std::move($3))); }
//| exp "%" exp   { $$.reset(new NumericMod(std::move($1), std::move($3))); }
%%

void logic::yy::Parser::error (const location_type& l, const std::string& m)
{
  driver.error (l, m);
}
