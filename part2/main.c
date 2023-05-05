#include "cc.h"
#include "cc.tab.h"
#include <stdio.h>
#include <stdlib.h>

extern relation *R;

void pindent(int x) { for (int i = 0; i < x; i++) putchar(' ');}

void yyerror(char *s, ...) {
}

void print_expression(expression *exp, int indent) {
    pindent(indent);
    if (exp->type == string) printf("%s", exp->data.str);
    else printf("%d", exp->data.value);
}

void print_predicate(predicate *P) {
    switch (P->type) {
        case atomic:
            printf("%s %s ", P->data.eins.attr, P->data.eins.relop);
            print_expression(P->data.eins.exp, 0); 
            break;
        case _and:
            printf("(");
            print_predicate(P->data.zwei.A);
            printf(") and (");
            print_predicate(P->data.zwei.B);
            printf(")");
            break;
        case _or:
            printf("(");
            print_predicate(P->data.zwei.A);
            printf(") or (");
            print_predicate(P->data.zwei.B);
            printf(")");
            break;
        case single:
            print_predicate(P->data.child);
            break;
    }
}

static void print_attributes(set *S) {
    printf("(attributes: [");
    for (set *ptr = S->next; ptr; ptr = ptr->next) {
        printf("%s", (char*)(ptr->item));
        if (ptr->next) printf(", ");
    }
    printf("])\n");
}

void print_attrs(attributes *attr) {
    if (attr->next == NULL) {
        printf("%s", attr->attr);
    }
    else {
        printf("%s, ", attr->attr);
        print_attrs(attr->next);
    }
}

void print_relation(relation *R, int indent) {
    pindent(indent);
    switch (R->type) {
        case P:
            printf("project ");
            print_attributes(R->attributes);
            pindent(indent + 2);
            printf("[");
            print_attrs(R->data.project.attrs);
            printf("]\n");
            print_relation(R->data.project.R ,indent + 2);
            break;
        case S:
            printf("select ");
            print_attributes(R->attributes);
            pindent(indent + 2);
            print_predicate(R->data.select.P);
            printf("\n");
            print_relation(R->data.select.R, indent + 2);
            break;
        case J:
            printf("join ");
            print_attributes(R->attributes);
            print_relation(R->data.join.R, indent + 2);
            print_relation(R->data.join.S, indent + 2);
            break;
        case TABLE:
            printf("%s ", R->data.table_name);
            print_attributes(R->attributes);
            break;
    }
    
}

void init_tables(int config) {
    tables = newset("");
    if (config <= 2) {
        set *employee, *department;
        employee = newset("EMPLOYEE"), department = newset("DEPARTMENT");
        insert(employee, "ENAME");
        insert(employee, "EID");
        insert(employee, "DID");
        insert(employee, "BDATE");
        insert(department, "DID");
        insert(department, "DNAME");
        insert(tables, employee);
        insert(tables, department);
    }
    else if (config >= 3) {
        set *works_on, *project;
        works_on = newset("WORKS_ON"), project = newset("PROJECT");
        insert(works_on, "ESSN");
        insert(works_on, "PID");
        insert(works_on, "BEGIN_DATE");
        insert(works_on, "END_DATE");
        insert(project, "PNAME");
        insert(project, "PID");
        insert(tables, works_on);
        insert(tables, project);
    }
    printf("init tables\n");
}

relation *expand_selects(relation *R) {
    switch (R->type) {
        case TABLE:
            break;
        case P:
            R->data.project.R = expand_selects(R->data.project.R);
            break;
        case S:
            if (R->data.select.P->type == _and) { // can be expanded
                relation *expand = malloc(sizeof(relation));
                expand->type = S;
                expand->attributes = copy(R->attributes);
                expand->data.select.R = R->data.select.R;
                expand->data.select.P = R->data.select.P->data.zwei.A;
                R->data.select.P = R->data.select.P->data.zwei.B;
                R->data.select.R = expand;
                R = expand_selects(R);
            }
            else {
                R->data.select.R = expand_selects(R->data.select.R);
            }
            break;
        case J:
            R->data.join.R = expand_selects(R->data.join.R);
            R->data.join.S = expand_selects(R->data.join.S);
            break;
    }
    return R;
}

int all_in(predicate *P, relation *R) {
    switch (P->type) {
        case atomic:
            return has_str(R->attributes, P->data.eins.attr);
        case single:
            return all_in(P->data.child, R);
        case _and:
            return all_in(P->data.zwei.A, R) && all_in(P->data.zwei.B, R);
        case _or:
            return all_in(P->data.zwei.A, R) && all_in(P->data.zwei.B, R);
    }
}

int attr_all_in(attributes *attr, relation *R) {
    if (attr->next == NULL) return has_str(R->attributes, attr->attr);
    return has_str(R->attributes, attr->attr) && attr_all_in(attr->next, R);
}

void pushdown_selects(relation *R) {
    switch (R->type) {
        case TABLE:
            break;
        case P:
            pushdown_selects(R->data.project.R);
            break;
        case S:
            pushdown_selects(R->data.select.R);
            break;
        case J:
            pushdown_selects(R->data.join.R);
            pushdown_selects(R->data.join.S);
            break;
    }
    relation *target, *r, *s, *select;
    predicate *p;
    switch (R->type) {
        case TABLE:
            break;
        case P:
            R->data.project.R = R->data.project.R;
            break;
        case S:
            target = R->data.select.R;
            p = R->data.select.P;
            r = target->data.join.R;
            s = target->data.join.S;
            if (target->type == J && p->type <= 1 &&
                (all_in(p, r) ^ all_in(p, s))) {
                if (all_in(p, r)) {
                    select = malloc(sizeof(relation));
                    *select = *R;
                    select->attributes = copy(target->data.join.R->attributes);
                    *R = *target; // root -> join
                    R->attributes = copy(target->attributes);
                    select->data.select.R = target->data.join.R;
                    R->data.join.R = select;
                }
                else {
                    select = malloc(sizeof(relation));
                    *select = *R;
                    select->attributes = copy(target->data.join.S->attributes);
                    *R = *target; // root -> join
                    R->attributes = copy(target->attributes);
                    select->data.select.R = target->data.join.S;
                    R->data.join.S = select;
                    
                }
                // R = target;
            }
            else {
                R->data.select.R = R->data.select.R;
            }
            break;
        case J:
            R->data.join.R = R->data.join.R;
            R->data.join.S = R->data.join.S;
            break;
    }
}

set *to_set(attributes *attrs) {
    set *s = newset("");
    attributes *ptr = attrs;
    while (ptr) {
        insert(s, ptr->attr);
        ptr = ptr->next;
    }
    return s;
}

set *from_predicate(predicate *P) {
    if (P->type == atomic) {
        set *s = newset("");
        insert(s, P->data.eins.attr);
        return s;
    }
    else if (P->type == _and || P->type == _or) return join(from_predicate(P->data.zwei.A), from_predicate(P->data.zwei.B));
    return from_predicate(P->data.child);
}

attributes *to_attrs(set *S) {
    set *ptr = S->next;
    attributes *attrs = malloc(sizeof(attributes)), *now = attrs;
    while (ptr) {
        now->attr = ptr->item;
        now->next = (ptr->next == NULL)? NULL: malloc(sizeof(attributes));
        now = now->next;
        ptr = ptr->next;
    }
    return attrs;
}

relation *pushdown_projects(relation *R) {
    relation *child;
    switch (R->type) {
        case TABLE:
            break;
        case P:
            child = R->data.project.R;
            if (child->type == J) { // when child is JOIN
                set *inter = intersect(child->data.join.R->attributes, child->data.join.S->attributes);
                set *P_attrs = to_set(R->data.project.attrs);
                set *R_attrs = join(inter, intersect(P_attrs, child->data.join.R->attributes));
                set *S_attrs = join(inter, intersect(P_attrs, child->data.join.S->attributes));
                relation *PR = malloc(sizeof(relation)), *PS = malloc(sizeof(relation));
                PR->type = PS->type = P;
                PR->attributes = R_attrs;
                PS->attributes = S_attrs;
                PR->data.project.attrs = to_attrs(R_attrs);
                PS->data.project.attrs = to_attrs(S_attrs);
                PR->data.project.R = child->data.join.R;
                PS->data.project.R = child->data.join.S;
                child->attributes = join(child->data.join.R->attributes, child->data.join.S->attributes);
                child->data.join.R = pushdown_projects(PR);
                child->data.join.S = pushdown_projects(PS);
                R->data.project.R = child;
            }
            else if (child->type == S) {
                set *new_pro_attrs = join(from_predicate(child->data.select.P), to_set(R->data.project.attrs));
                R->data.project.R = pushdown_projects(child->data.select.R);
                R->data.project.attrs = to_attrs(new_pro_attrs);
                child->data.select.R = R;
                child->attributes = copy(R->attributes);
                return child;
            }
            else {
                R->data.project.R = pushdown_projects(R->data.project.R);
            }
            break;
        case S:
            R->data.select.R = pushdown_projects(R->data.select.R);
            break;
        case J:
            R->data.join.R = pushdown_projects(R->data.join.R);
            R->data.join.S = pushdown_projects(R->data.join.S);
            break;
    }
    return R;
}

relation *optimize(relation *R) {
    R = expand_selects(R);
    pushdown_selects(R);
    R = pushdown_projects(R);
    return R;
}

int main(int argc, char **argv) {
    init_tables(1);
    FILE *f = fopen("query.txt", "r");
    yyrestart(f);
    yyparse();
    print_relation(R, 0);
    
    R = optimize(R);
    printf("\nAfter optimization:\n\n");
    
    print_relation(R, 0);
    
    fclose(f);
    return 0;
}
