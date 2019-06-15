%{ /* -*- C++ -*- */
# include <cerrno>
# include <climits>
# include <cstdlib>
# include <string>
# include "logic/Driver.hxx"
# include "logic/Parser.hxxout"

// Work around an incompatibility in flex (at least versions
// 2.5.31 through 2.5.33): it generates code that does
// not conform to C89.  See Debian bug 333231
// <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.
# undef yywrap
# define yywrap() 1

// The location of the current token.
static logic::yy::location loc;
%}
%option noyywrap nounput batch debug noinput
id    [a-zA-Z][a-zA-Z_0-9]*
int   [0-9]+
blank [ \t]

%{
  // Code run each time a pattern is matched.
  # define YY_USER_ACTION  loc.columns (yyleng);
%}

%%

%{
  // Code run each time yylex is called.
  loc.step ();
%}

{blank}+   loc.step ();
[\n]+      loc.lines (yyleng); loc.step ();
"-"      return logic::yy::Parser::make_MINUS(loc);
"+"      return logic::yy::Parser::make_PLUS(loc);
"*"      return logic::yy::Parser::make_STAR(loc);
"/"      return logic::yy::Parser::make_SLASH(loc);
"div"      return logic::yy::Parser::make_SLASH(loc);
"%"      return logic::yy::Parser::make_PERCENT(loc);
"mod"      return logic::yy::Parser::make_PERCENT(loc);
"("      return logic::yy::Parser::make_LPAREN(loc);
")"      return logic::yy::Parser::make_RPAREN(loc);
"="     return logic::yy::Parser::make_ASSIGN(loc);
"true"     return logic::yy::Parser::make_BOOL(true, loc);
"True"     return logic::yy::Parser::make_BOOL(true, loc);
"TRUE"     return logic::yy::Parser::make_BOOL(true, loc);
"thrown"     return logic::yy::Parser::make_BOOL(true, loc);
"Thrown"     return logic::yy::Parser::make_BOOL(true, loc);
"THROWN"     return logic::yy::Parser::make_BOOL(true, loc);
"active"     return logic::yy::Parser::make_BOOL(true, loc);
"Active"     return logic::yy::Parser::make_BOOL(true, loc);
"ACTIVE"     return logic::yy::Parser::make_BOOL(true, loc);

"false"     return logic::yy::Parser::make_BOOL(false, loc);
"FALSE"     return logic::yy::Parser::make_BOOL(false, loc);
"False"     return logic::yy::Parser::make_BOOL(false, loc);
"closed"     return logic::yy::Parser::make_BOOL(false, loc);
"Closed"     return logic::yy::Parser::make_BOOL(false, loc);
"CLOSED"     return logic::yy::Parser::make_BOOL(false, loc);
"inactive"     return logic::yy::Parser::make_BOOL(false, loc);
"Inactive"     return logic::yy::Parser::make_BOOL(false, loc);
"InActive"     return logic::yy::Parser::make_BOOL(false, loc);
"INACTIVE"     return logic::yy::Parser::make_BOOL(false, loc);

{int}      {
  errno = 0;
  long n = strtol (yytext, NULL, 10);
  if (! (INT_MIN <= n && n <= INT_MAX && errno != ERANGE))
    driver.error (loc, "integer is out of range");
  return logic::yy::Parser::make_NUMBER(n, loc);
}

{id}       return logic::yy::Parser::make_IDENTIFIER(yytext, loc);
.          driver.error (loc, "invalid character");
<<EOF>>    return logic::yy::Parser::make_END(loc);
%%

void
logic::Driver::scan_begin ()
{
  yy_flex_debug = debug_level_ & TRACE_LEX ? true : false;
  if (filename_.empty () || filename_ == "-")
    yyin = stdin;
  else if (!(yyin = fopen (filename_.c_str (), "r")))
    {
      error ("cannot open " + filename_ + ": " + strerror(errno));
      exit (EXIT_FAILURE);
    }
}



void
logic::Driver::scan_end ()
{
  fclose (yyin);
}

