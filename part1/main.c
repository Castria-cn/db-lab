#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "relation.h"

int main() {
    relation_info R = init_relation(112, 8, R_generator);
    show_relation(R, "R(A, B)");
    relation_info S = init_relation(224, 8, S_generator);
    show_relation(S, "S(C, D)");
    relation_info select_R = select(R, 0, equals, 40);
    show_relation(select_R, "select_R(A)");
    relation_info project_R = project(R, 0);
    show_relation(project_R, "project_R(A)");
    relation_info RJS = sort_merge_join(R, S, 0, 0);
    show_relation(RJS, "R.A JOIN S.C(A, B, C, D)");
    return 0;
}