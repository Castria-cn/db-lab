%option noyywrap nodefault
%{
#include "cc.h"
#include "cc.tab.h"
%}

wspace   [ \t\n]
letter   [_A-Za-z]
digit    [0-9]
lp       \(
rp       \)
lb       \[
rb       \]
comma    [,]
and      &
or       \|
relop    >|<|(>=)|(<=)|(=)|(!=)

ws       {wspace}+
int      {digit}+
id       {letter}({letter}|{digit})*
string   '[^']+'

%%
{ws}         {}
"SELECT"     { return SELECT; }
"PROJECTION" { return PROJECT; }
"JOIN"       { return JOIN; }
{lp}         { return LP; }
{rp}         { return RP; }
{lb}         { return LB; }
{rb}         { return RB; }
{comma}      { return COMMA; }
{string}     { yylval.id = malloc(strlen(yytext)); strcpy(yylval.id, yytext); return STRING; }
{relop}      { yylval.id = malloc(strlen(yytext)); strcpy(yylval.id, yytext); return RELOP; }
{and}        { return AND; }
{or}         { return OR; }
{int}        { yylval.num = atoi(yytext); return INT; }
{id}         { yylval.id = malloc(strlen(yytext)); strcpy(yylval.id, yytext); return ID; }
.            {}
%%
