%pure-parser
%lex-param {yyscan_t scanner}
%parse-param {yyscan_t scanner}
%parse-param {parse_data_t *data}

%{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "config_bison.h"
#include "config_flex.h"

typedef struct {
    bool user_set;
    bool group_set;
    bool umask_set;
    config_callback_t *cb;
    void *priv;
} parse_data_t;

static void lxc_genesis_yyerror(yyscan_t scan, parse_data_t *data,
        const char *s)
{
    printf("ERROR: %s at line %d\n", s, lxc_genesis_yyget_lineno(scan));
}

#ifdef CONFIG_DEBUG
# define YYDEBUG 1
# define YYERROR_VERBOSE 1
#endif

%}

%union {
    int ival;
    char *sval;
}

%token ASSIGNMENT
%token RLIMIT_SEP

%token USER
%token GROUP
%token UMASK
%token RLIMIT
%token EOL

%token <sval> RLIMIT_RES
%token <ival> UMASK_VAL
%token <sval> RLIMIT_VAL
%token <sval> STRING
%token <sval> IDENTIFIER

%start config

%%

config:
    config_entries

config_entries:
    /* empty */ | config_entries config_entry

config_entry:
    empty | user | group | umask | env_var | rlimit

empty:
    EOL {}

user:
    USER ASSIGNMENT STRING EOL {
        bool success;
        config_entry_t entry;

        if (data->user_set) {
            fprintf(stderr, "user can be defined only once!\n");
            YYABORT;
        }
        data->user_set = true;

        entry.type = CONFIG_ENTRY_USER;
        entry.u.user = $3;
        success = data->cb(&entry, data->priv);
        free($3);
        if (!success)
            YYABORT;
    }

group:
    GROUP ASSIGNMENT STRING EOL {
        bool success;
        config_entry_t entry;

        if (data->group_set) {
            fprintf(stderr, "group can be defined only once!\n");
            YYABORT;
        }
        data->group_set = true;

        entry.type = CONFIG_ENTRY_GROUP;
        entry.u.group = $3;
        success = data->cb(&entry, data->priv);
        free($3);
        if (!success)
            YYABORT;
    }

umask:
    UMASK ASSIGNMENT UMASK_VAL EOL {
        config_entry_t entry;

        if (data->umask_set) {
            fprintf(stderr, "umask can be defined only once!\n");
            YYABORT;
        }
        data->umask_set = true;

        entry.type = CONFIG_ENTRY_UMASK;
        entry.u.umask = $3;
        if (!data->cb(&entry, data->priv))
            YYABORT;
    }

rlimit:
    RLIMIT ASSIGNMENT RLIMIT_RES RLIMIT_SEP RLIMIT_VAL RLIMIT_SEP RLIMIT_VAL EOL {
        bool success;
        config_entry_t entry;

        entry.type = CONFIG_ENTRY_RLIMIT;
        entry.u.rlimit.name = $3;
        entry.u.rlimit.current = $5;
        entry.u.rlimit.maximum = $7;
        success = data->cb(&entry, data->priv);
        free($3);
        free($5);
        free($7);
        if (!success)
            YYABORT;
    }

env_var:
    IDENTIFIER ASSIGNMENT STRING EOL {
        bool success;
        config_entry_t entry;

        entry.type = CONFIG_ENTRY_ENV;
        entry.u.env.name = $1;
        entry.u.env.value = $3;
        success = data->cb(&entry, data->priv);
        free($1);
        free($3);
        if (!success)
            YYABORT;
    }

%%

bool lxc_genesis_config_load(FILE *f, config_callback_t *cb, void *priv)
{
    int result;
    yyscan_t scanner;
    parse_data_t data;

    data.user_set = data.group_set = data.umask_set = false;
    data.cb = cb;
    data.priv = priv;

    lxc_genesis_yylex_init(&scanner);
#ifdef CONFIG_DEBUG
    lxc_genesis_yyset_debug(1, scanner);
    lxc_genesis_yydebug = 1;
#endif
    lxc_genesis_yyset_in(f, scanner);
    result = lxc_genesis_yyparse(scanner, &data);
    lxc_genesis_yylex_destroy(scanner);

    return (result == 0);
}
