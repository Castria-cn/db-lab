#include "set.h"
#include <stdio.h>

extern int yylineno;

void yyerror(char *s, ...);

void yyrestart(FILE *f);

int yylex_destroy(void);

int yylex();

enum exp_type  { string, _int };
enum pred_type { atomic, single, _and, _or };
enum operation { P, S, J, TABLE };

typedef struct attributes {
   char *attr;
   struct attributes *next;
} attributes;

typedef struct {
    int type;
    union {
        char *str;
        int value;
    } data;
} expression;

typedef struct predicate {
   int type; // 0 -- atomic, 1 -- single, 2 -- and, 3 -- or
   union {
       struct predicate *child;
       struct { char *attr, *relop; expression *exp; } eins;
       struct { struct predicate *A, *B; } zwei;
   } data;
} predicate;

typedef struct relation {
    enum operation type;
    set *attributes;
    union {
        char *table_name; // for a table
        struct { struct relation *R, *S; } join;// for join
        struct { struct relation *R; predicate *P; } select; // for select
        struct { struct relation *R; attributes *attrs; } project; // for project
    } data;
} relation;

relation *R;

set *tables;
