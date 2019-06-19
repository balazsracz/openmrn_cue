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
  SEMICOLON  ";"
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
;
%token <std::string> IDENTIFIER "identifier"
%token <int> NUMBER "number"
%token <bool> BOOL "constbool"
%type  <std::shared_ptr<logic::NumericExpression> > exp
%type  <std::shared_ptr<logic::BooleanExpression> > boolexp
%type  <std::shared_ptr<logic::Command> > command
%type  <std::shared_ptr<logic::Command> > assignment
%type  <std::shared_ptr<logic::Command> > variable_decl
%type  <std::shared_ptr<std::vector<std::shared_ptr<logic::Command> > > > commands
//%printer { yyoutput << $$; } <*>;
%%
%start unit;
unit: commands { driver.commands_.swap(*$1); };

assignment:
"identifier" "=" exp {
  const Symbol* s = driver.find_symbol($1);
  if (!s) {
    string err = "Undeclared variable '";
    err += $1;
    err += "'";
    driver.error(@1, err);
    YYERROR;
  } else if (s->symbol_type_ != Symbol::LOCAL_VAR_INT) {
    string err = "'";
    err += $1;
    err += "' incorrect type; expected int";
    driver.error(@1, err);
    YYERROR;
  } else {
    $$.reset(new NumericAssignment($1, *s, std::move($3)));
  }
}
| "identifier" "=" boolexp {
  const Symbol* s = driver.find_symbol($1);
  if (!s) {
    string err = "Undeclared variable '";
    err += $1;
    err += "'";
    driver.error(@1, err);
    YYERROR;
  } else if (s->symbol_type_ != Symbol::LOCAL_VAR_BOOL) {
    string err = "'";
    err += $1;
    err += "' incorrect type; expected bool";
    driver.error(@1, err);
    YYERROR;
  } else {
    $$.reset(new BooleanAssignment($1, *s, std::move($3)));
  }
}

;

variable_decl:
"int" "identifier" ";" {
  if (!driver.allocate_variable($2, Symbol::LOCAL_VAR_INT)) {
    YYERROR;
  }
  // No code to output.
  $$ = std::make_shared<EmptyCommand>();
  }
| "bool" "identifier" ";" {
  if (!driver.allocate_variable($2, Symbol::LOCAL_VAR_BOOL)) {
    YYERROR;
  }
  // No code to output.
  $$ = std::make_shared<EmptyCommand>();
  }
| "int" "identifier" "=" exp ";" {
  if (!driver.allocate_variable($2, Symbol::LOCAL_VAR_INT)) {
    YYERROR;
  } else {

  }
  // No code to output.
  $$ = std::make_shared<NumericAssignment>($2, *driver.find_symbol($2), std::move($4));
  }
/*| "bool" IDENTIFIER ";" {
  driver.allocate_variable($2, LOCAL_VAR_BOOL);
  // No code to output.
  $$ = std::make_shared<EmptyCommand>();
  }
*/
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
%left "*" "/";
exp:
  exp "+" exp   { $$.reset(new NumericAdd(std::move($1), std::move($3))); }
| "(" exp ")"   { std::swap ($$, $2); }
| "number"      { $$.reset(new NumericConstant($1)); }
| "identifier"  { $$ = std::make_shared<NumericVariable>(std::move($1)); }
;

%left "==" "!=";
%left "&&";
%left "||";

boolexp:
  "constbool"      { $$ = std::make_shared<BooleanConstant>($1); }
| "(" boolexp ")"   { std::swap ($$, $2); }
|  boolexp "&&" boolexp   { $$ = std::make_shared<BoolAnd>(std::move($1), std::move($3)); }
|  boolexp "||" boolexp   { $$ = std::make_shared<BoolOr>(std::move($1), std::move($3)); }
;

//| exp "-" exp   { $$.reset(new NumericMinus(std::move($1), std::move($3))); }
//| exp "*" exp   { $$.reset(new NumericMul(std::move($1), std::move($3))); }
//| exp "/" exp   { $$.reset(new NumericDiv(std::move($1), std::move($3))); }
//| exp "%" exp   { $$.reset(new NumericMod(std::move($1), std::move($3))); }
%%

void logic::yy::Parser::error (const location_type& l, const std::string& m)
{
  driver.error (l, m);
}
