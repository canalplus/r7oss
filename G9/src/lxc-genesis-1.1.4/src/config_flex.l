%x S_ASSIGN
%x S_STRING
%x S_UMASK
%x S_RLIMIT
%x END

%option bison-bridge
%option noinput
%option nounput
%option noyywrap
%option reentrant
%option stack
%option yylineno
%{
#include <stdio.h>
#include <stdlib.h>

#include "config_bison.h"
#include "utils.h"

#pragma GCC diagnostic ignored "-Wunused-function"
%}

%%
<*>#.* ;

<INITIAL>user {
    yy_push_state(S_STRING, yyscanner);
    yy_push_state(S_ASSIGN, yyscanner);
    return USER;
}

<INITIAL>group {
    yy_push_state(S_STRING, yyscanner);
    yy_push_state(S_ASSIGN, yyscanner);
    return GROUP;
}

<INITIAL>umask {
    yy_push_state(S_UMASK, yyscanner);
    yy_push_state(S_ASSIGN, yyscanner);
    return UMASK;
}

<INITIAL>rlimit {
    yy_push_state(S_RLIMIT, yyscanner);
    yy_push_state(S_ASSIGN, yyscanner);
    return RLIMIT;
}

<INITIAL>[[:graph:]]{-}[=]+ {
    yy_push_state(S_STRING, yyscanner);
    yy_push_state(S_ASSIGN, yyscanner);
    yylval->sval = strdup(yytext);
    return IDENTIFIER;
}

<S_ASSIGN>[=] {
    yy_pop_state(yyscanner);
    return ASSIGNMENT;
}

<S_UMASK>[0-7][0-7][0-7] {
    yylval->ival = strtol(yytext, NULL, 8);
    return UMASK_VAL;
}

<S_RLIMIT>[|] {
    return RLIMIT_SEP;
}
<S_RLIMIT>RLIMIT[_][A-Z]+ {
    yylval->sval = lxc_genesis_strdup(yytext);
    return RLIMIT_RES;
}
<S_RLIMIT>[0-9]+ {
    yylval->sval = strdup(yytext);
    return RLIMIT_VAL;
}

<S_STRING>[[:graph:]]+ {
    yylval->sval = strdup(yytext);
    return STRING;
}

<*>[\r\n]+ {
    BEGIN(INITIAL);
    return EOL;
}

<*>[[:blank:]]+ ;

<END><<EOF>> {
    yyterminate();
}

<<EOF>> {
    BEGIN(END);
    return EOL;
}

<*>. {
    return yytext[0];
}
