%{
#include "cc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *cpystr(char *s) {
    char *ret = malloc(strlen(s));
    strcpy(ret, s);
    return ret;
}

set *get_attributes(attributes *attr) {
    set *ret = newset("");
    insert(ret, attr->attr);
    if (attr->next == NULL) return ret;
    return join(ret, get_attributes(attr->next)); 
}

static void print_attributes(set *S) {
    printf("(attributes: [");
    for (set *ptr = S->next; ptr; ptr = ptr->next) {
        printf("%s", (char*)(ptr->item));
        if (ptr->next) printf(", ");
    }
    printf("])\n");
}
%}

%union {
    char *id;
    int num;
    relation *rel;
    predicate *pred;
    expression *exp;
    attributes *attrs;
};

%token <id> ID RELOP STRING
%token <num> INT
%token SELECT PROJECT JOIN LP RP LB RB COMMA AND OR

%type <rel> Relation Start
%type <pred> Predicate SinglePredicate
%type <exp> Exp
%type <attrs> Attributes

%%
Start: Relation { R = $$; };
Relation:
PROJECT LB Attributes RB LP Relation RP {
    $$ = malloc(sizeof(relation));
    $$->type = P;
    $$->data.project.R = $6;
    $$->data.project.attrs = $3;
    $$->attributes = get_attributes($3);
}
| SELECT LB Predicate RB LP Relation RP {
    $$ = malloc(sizeof(relation));
    $$->type = S;
    $$->data.select.R = $6;
    $$->data.select.P = $3;
    $$->attributes = copy($6->attributes);
}
| Relation JOIN Relation {
    $$ = malloc(sizeof(relation));
    $$->type = J;
    $$->data.join.R = $1;
    $$->data.join.S = $3;
    $$->attributes = join($1->attributes, $3->attributes);
}
| ID {
    $$ = malloc(sizeof(relation));
    $$->type = TABLE;
    $$->data.table_name = cpystr($1);
    $$->attributes = copy(find_by_name(tables, $1));
};

Attributes:
ID {
    $$ = malloc(sizeof(attributes));
    $$->attr = cpystr($1);
    $$->next = NULL;
}
| ID COMMA Attributes {
    $$ = malloc(sizeof(attributes));
    $$->attr = cpystr($1);
    $$->next = $3;
};

Predicate:
SinglePredicate {
    $$ = malloc(sizeof(predicate));
    $$->type = single;
    $$->data.child = $1;
}
| SinglePredicate AND Predicate {
    $$ = malloc(sizeof(predicate));
    $$->type = _and;
    $$->data.zwei.A = $1;
    $$->data.zwei.B = $3;
}
| SinglePredicate OR Predicate {
    $$ = malloc(sizeof(predicate));
    $$->type = _or;
    $$->data.zwei.A = $1;
    $$->data.zwei.B = $3;
};

SinglePredicate:
ID RELOP Exp {
    $$ = malloc(sizeof(predicate));
    $$->type = atomic;
    $$->data.eins.attr = cpystr($1);
    $$->data.eins.relop = cpystr($2);
    $$->data.eins.exp = $3;
};

Exp:
INT {
    $$ = malloc(sizeof(expression));
    $$->type = _int;
    $$->data.value = $1;
}
| STRING {
    $$ = malloc(sizeof(expression));
    $$->type = string;
    $$->data.str = cpystr($1);
};
%%
