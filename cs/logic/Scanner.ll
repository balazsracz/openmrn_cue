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
 * \file Scanner.ll
 *
 * Flex command file for the lexical analyzer of the programming language.
 *
 * @author Balazs Racz
 * @date 15 June 2019
 */

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
string \"(\\.|[^"\\])*\"
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

"//".*     {  }
{blank}+   loc.step ();
[\n]+      loc.lines (yyleng); loc.step ();
[\000\377] return logic::yy::Parser::make_END(loc);
"-"        return logic::yy::Parser::make_MINUS(loc);
"+"        return logic::yy::Parser::make_PLUS(loc);
"*"        return logic::yy::Parser::make_STAR(loc);
"/"        return logic::yy::Parser::make_SLASH(loc);
"div"      return logic::yy::Parser::make_SLASH(loc);
"%"        return logic::yy::Parser::make_PERCENT(loc);
"mod"      return logic::yy::Parser::make_PERCENT(loc);
"("        return logic::yy::Parser::make_LPAREN(loc);
")"        return logic::yy::Parser::make_RPAREN(loc);
"{"        return logic::yy::Parser::make_LBRACE(loc);
"}"        return logic::yy::Parser::make_RBRACE(loc);
";"        return logic::yy::Parser::make_SEMICOLON(loc);
","        return logic::yy::Parser::make_COMMA(loc);
"="        return logic::yy::Parser::make_ASSIGN(loc);
"&&"       return logic::yy::Parser::make_DOUBLEAND(loc);
"and"      return logic::yy::Parser::make_DOUBLEAND(loc);
"||"       return logic::yy::Parser::make_DOUBLEOR(loc);
"or"       return logic::yy::Parser::make_DOUBLEOR(loc);
"!"        return logic::yy::Parser::make_BANG(loc);
"not"      return logic::yy::Parser::make_BANG(loc);
"=="       return logic::yy::Parser::make_DOUBLEEQ(loc);
"is"       return logic::yy::Parser::make_DOUBLEEQ(loc);
"if"       return logic::yy::Parser::make_IF(loc);
"else"     return logic::yy::Parser::make_ELSE(loc);

"int"      return logic::yy::Parser::make_TYPEINT(loc);
"bool"     return logic::yy::Parser::make_TYPEBOOL(loc);

"exported" return logic::yy::Parser::make_EXPORTED(loc);  
"auto" return logic::yy::Parser::make_AUTO(loc);  

"print" return logic::yy::Parser::make_PRINT(loc);
"terminate" return logic::yy::Parser::make_TERMINATE(loc);

"true"     return logic::yy::Parser::make_BOOL(true, loc);
"True"     return logic::yy::Parser::make_BOOL(true, loc);
"TRUE"     return logic::yy::Parser::make_BOOL(true, loc);
"yes"      return logic::yy::Parser::make_BOOL(true, loc);
"Yes"      return logic::yy::Parser::make_BOOL(true, loc);
"YES"      return logic::yy::Parser::make_BOOL(true, loc);
"thrown"     return logic::yy::Parser::make_BOOL(true, loc);
"Thrown"     return logic::yy::Parser::make_BOOL(true, loc);
"THROWN"     return logic::yy::Parser::make_BOOL(true, loc);
"active"     return logic::yy::Parser::make_BOOL(true, loc);
"Active"     return logic::yy::Parser::make_BOOL(true, loc);
"ACTIVE"     return logic::yy::Parser::make_BOOL(true, loc);
"on"       return logic::yy::Parser::make_BOOL(true, loc);
"On"       return logic::yy::Parser::make_BOOL(true, loc);
"ON"       return logic::yy::Parser::make_BOOL(true, loc);

"false"     return logic::yy::Parser::make_BOOL(false, loc);
"FALSE"     return logic::yy::Parser::make_BOOL(false, loc);
"False"     return logic::yy::Parser::make_BOOL(false, loc);
"no"        return logic::yy::Parser::make_BOOL(false, loc);
"No"        return logic::yy::Parser::make_BOOL(false, loc);
"NO"        return logic::yy::Parser::make_BOOL(false, loc);
"closed"     return logic::yy::Parser::make_BOOL(false, loc);
"Closed"     return logic::yy::Parser::make_BOOL(false, loc);
"CLOSED"     return logic::yy::Parser::make_BOOL(false, loc);
"inactive"     return logic::yy::Parser::make_BOOL(false, loc);
"Inactive"     return logic::yy::Parser::make_BOOL(false, loc);
"InActive"     return logic::yy::Parser::make_BOOL(false, loc);
"INACTIVE"     return logic::yy::Parser::make_BOOL(false, loc);
"off"     return logic::yy::Parser::make_BOOL(false, loc);
"Off"     return logic::yy::Parser::make_BOOL(false, loc);
"OFF"     return logic::yy::Parser::make_BOOL(false, loc);

{int}      {
  errno = 0;
  long n = strtol (yytext, NULL, 10);
  if (! (INT_MIN <= n && n <= INT_MAX && errno != ERANGE))
    driver.error (loc, "integer is out of range");
  return logic::yy::Parser::make_NUMBER(n, loc);
}

{string} {
  std::string output;
  output.reserve(strlen(yytext));  
  const char *c = yytext;
  if ((*c) != '"') {
    driver.error(loc, "string must start with quotes");
    return logic::yy::Parser::make_STRING("", loc);
  }
  ++c;
  while ((*c) && !((*c) == '"')) {
    if ((*c) == '\\') {
      ++c;
      if (!*c) {
        driver.error(loc, "unexpected 0 inside string");
        return logic::yy::Parser::make_STRING("", loc);
      }
    }
    output.push_back(*c);
    ++c;
  }  
  return logic::yy::Parser::make_STRING(output, loc);
}

{id} {
  auto* symbol = driver.find_symbol(yytext);
  if (!symbol) {
    return logic::yy::Parser::make_UNDECL_ID(yytext, loc);
  }
  if (symbol->symbol_type_ == logic::Symbol::FUNCTION) {
    auto t = symbol->get_data_type();
    switch(t) {
      case logic::Symbol::DATATYPE_INT:
        return logic::yy::Parser::make_INT_FN_ID(yytext, loc);
      case logic::Symbol::DATATYPE_BOOL:
        return logic::yy::Parser::make_BOOL_FN_ID(yytext, loc);
      case logic::Symbol::DATATYPE_VOID:
        return logic::yy::Parser::make_VOID_FN_ID(yytext, loc);
    }
  } else {
    auto t = symbol->get_data_type();
    if (t == logic::Symbol::DATATYPE_INT) {
      return logic::yy::Parser::make_INT_VAR_ID(yytext, loc);
    } else if (t == logic::Symbol::DATATYPE_BOOL) {
      return logic::yy::Parser::make_BOOL_VAR_ID(yytext, loc);
    }
  }
  driver.error (loc, "unknown symbol type");
}
.          driver.error (loc, "invalid character");
<<EOF>>    return logic::yy::Parser::make_END(loc);
%%

void
logic::Driver::scan_begin ()
{
  yy_init_globals();
  yy_flex_debug = debug_level_ & TRACE_LEX ? true : false;
  if (filename_.empty () || filename_ == "-")
    yyin = stdin;
  else if (!(yyin = fopen (filename_.c_str (), "r")))
    {
      error ("cannot open " + filename_ + ": " + strerror(errno));
      exit (EXIT_FAILURE);
    }
  loc.initialize(&filename_, 1, 1);
}



void
logic::Driver::scan_end ()
{
  fclose (yyin);
}

