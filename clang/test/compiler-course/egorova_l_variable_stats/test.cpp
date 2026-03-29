// clang-format off
// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/egorova_l_variable_stats_ClangAST%pluginext -plugin egorova-variable-stats -fsyntax-only %t/basic.c 2>&1 | FileCheck %s --check-prefix=CHECK-BASIC
// RUN: %clang_cc1 -load %llvmshlibdir/egorova_l_variable_stats_ClangAST%pluginext -plugin egorova-variable-stats -fsyntax-only %t/static.c 2>&1 | FileCheck %s --check-prefix=CHECK-STATIC
// RUN: %clang_cc1 -load %llvmshlibdir/egorova_l_variable_stats_ClangAST%pluginext -plugin egorova-variable-stats -fsyntax-only %t/params.c 2>&1 | FileCheck %s --check-prefix=CHECK-PARAMS
// RUN: %clang_cc1 -load %llvmshlibdir/egorova_l_variable_stats_ClangAST%pluginext -plugin egorova-variable-stats -fsyntax-only %t/all_types.c 2>&1 | FileCheck %s --check-prefix=CHECK-ALL
// RUN: %clang_cc1 -load %llvmshlibdir/egorova_l_variable_stats_ClangAST%pluginext -plugin egorova-variable-stats -fsyntax-only %t/edge_cases.c 2>&1 | FileCheck %s --check-prefix=CHECK-EDGE
// RUN: %clang_cc1 -load %llvmshlibdir/egorova_l_variable_stats_ClangAST%pluginext -plugin egorova-variable-stats -fsyntax-only %t/declarations.c 2>&1 | FileCheck %s --check-prefix=CHECK-DECL

//--- basic.c
int g1 = 10;
int g2 = 20;
int test(int p) {
    int a = 30;
    int b = 40;
    return p + a + b;
}
// CHECK-BASIC: Global variables:        2
// CHECK-BASIC: Static global variables: 0
// CHECK-BASIC: Static local variables:  0
// CHECK-BASIC: Local variables:         2
// CHECK-BASIC: Function parameters:     1
// CHECK-BASIC: TOTAL:                   5

//--- static.c
int global = 100;
static int static_global1 = 200;
static int static_global2 = 300;
void func1(int p1, float p2) {
    int local1 = 400;
    static int static_local1 = 500;
    {
        int block_local = 600;
        local1 = block_local;
    }
}
void func2(void) {
    static int static_local2 = 700;
    static int static_local3 = 800;
    int local2 = 900;
}
// CHECK-STATIC: Global variables:        1
// CHECK-STATIC: Static global variables: 2
// CHECK-STATIC: Static local variables:  3
// CHECK-STATIC: Local variables:         3
// CHECK-STATIC: Function parameters:     2
// CHECK-STATIC: TOTAL:                   11

//--- params.c
void func1(int a, char b, double c) {}
void func2(long d, short e) {}
// CHECK-PARAMS: Global variables:        0
// CHECK-PARAMS: Static global variables: 0
// CHECK-PARAMS: Static local variables:  0
// CHECK-PARAMS: Local variables:         0
// CHECK-PARAMS: Function parameters:     5
// CHECK-PARAMS: TOTAL:                   5

//--- all_types.c
int global_int = 42;
float global_float = 3.14f;
static char static_global_char = 'A';
int complex_test(int p1, float p2, char p3, double p4) {
    int local1 = 10;
    static double static_local1 = 3.14159;
    {
        int local2 = 20;
        static char static_local2 = 'B';
        float local3 = 30.0f;
        local1 = local2 + local3;
    }
    return p1 + local1;
}
// CHECK-ALL: Global variables:        2
// CHECK-ALL: Static global variables: 1
// CHECK-ALL: Static local variables:  2
// CHECK-ALL: Local variables:         3
// CHECK-ALL: Function parameters:     4
// CHECK-ALL: TOTAL:                   12

//--- edge_cases.c
int global_array[10];
static struct { int x; } static_struct;
void test_edge(int param) {
    int local_array[5];
    static int* static_ptr = 0;
    local_array[0] = param;
    static_ptr = local_array;
}
// CHECK-EDGE: Global variables:        1
// CHECK-EDGE: Static global variables: 1
// CHECK-EDGE: Static local variables:  1
// CHECK-EDGE: Local variables:         1
// CHECK-EDGE: Function parameters:     1
// CHECK-EDGE: TOTAL:                   5

//--- declarations.c
int value = 42;            // + 1 global
extern int externValue;    // Пропускаем
void foo(int p);           // Пропускаем (прототип)
void boo(float f, int i) { // + 1 function, + 2 params
    int local = 10;        // + 1 local
}
// CHECK-DECL: Global variables:        1
// CHECK-DECL: Static global variables: 0
// CHECK-DECL: Static local variables:  0
// CHECK-DECL: Local variables:         1
// CHECK-DECL: Function parameters:     2
// CHECK-DECL: TOTAL:                   4