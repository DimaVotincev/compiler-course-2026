// RUN: %clang_cc1 -load %llvmshlibdir/volkov_a_lab1_task4_ClangAST%pluginext -plugin volkov_a_var_statistic -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK: Total count : 26
// CHECK-NEXT: Global variables : 6
// CHECK-NEXT: Static variables : 7
// CHECK-NEXT: Local variables  : 6
// CHECK-NEXT: Function params  : 7

extern int extern_global_1; // не считается (+0 global)
extern int extern_global_2; // не считается (+0 global)
int foo(int a, int b);      // не считается (+0 params)

int alpha = 1;         // global: 1
extern double beta;    // не считается (только объявление)
static char gamma;     // static: 1

namespace module_a {
    extern double beta; // не считается (только объявление)
    static int delta;   // static: 2
    int epsilon;        // global: 2
}

namespace module_a {
    extern int epsilon; // не считается (только объявление)
}

namespace {
    int zeta = 0;       // global: 3 (анонимный namespace без static - расценивается как global)
    static float eta;   // static: 3
}

extern double beta;     // не считается (только объявление)
extern int alpha;       // не считается (только объявление)

int global_x;           // global: 4
int global_y;           // global: 5
int global_z;           // global: 6

struct data_point {
    data_point(int x, int y) {} // param: 1, param: 2
    int data_x; // поле структуры (не var_decl)
    int data_y; // поле структуры (не var_decl)
};

template<typename T>
T algorithm(T input1, T input2) { // param: 3, param: 4
    static T state;          // static: 4
    T intermediate = input1; // local: 1
    return intermediate;
}

long compute_value(long val) { // param: 5
    int tmp1 = val * 2;        // local: 2
    int tmp2 = tmp1 + 1;       // local: 3
    return tmp2;
}

int main(int argc, char** argv) {     // param: 6, param: 7
    static data_point dp_static(0, 0); // static: 5
    data_point dp_local(1, 1);         // local: 4
    
    static constexpr int const_var = 100; // static: 6
    constexpr float const_flt = 3.14f;    // local: 5
    
    long res = compute_value(10L);        // local: 6
    
    static int final_stat = 42;           // static: 7
    
    return 0;
}