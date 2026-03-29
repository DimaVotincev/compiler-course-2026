// RUN: %clang_cc1 -load %llvmshlibdir/baldin_a_lab1_ClangAST%pluginext -plugin const_plugin -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK-LABEL: test_full_const
// CHECK: const int* const p1 {{=}}
void test_full_const() {
    int a = 10;
    int* p1 = &a;
    int b = *p1;
}

// CHECK-LABEL: test_const_data
// CHECK: const int* p2 {{=}}
void test_const_data() {
    int a = 10;
    int* p2 = &a;
    p2++; 
}

// CHECK-LABEL: test_const_pointer
// CHECK: int* const p3 {{=}}
void test_const_pointer() {
    int a = 10;
    int* p3 = &a;
    *p3 = 20; 
}

// CHECK-LABEL: test_reference
// CHECK: const int& ref {{=}}
void test_reference() {
    int a = 10;
    int& ref = a;
    int c = ref;
}

// CHECK-LABEL: test_no_const
// CHECK: int* p4 {{=}}
// CHECK-NOT: const int * p4
void test_no_const() {
    int a = 10;
    int* p4 = &a;
    p4++;
    *p4 = 30;
}

// CHECK-LABEL: test_already_const
// CHECK: const int* const p5 {{=}}
void test_already_const() {
    int a = 10;
    const int* const p5 = &a;
    int b = *p5;
}

// CHECK-LABEL: test_array_mutation
// CHECK: int* const p6 {{=}}
void test_array_mutation() {
    int arr[5] = {1, 2, 3, 4, 5};
    int* p6 = arr;
    p6[2] = 10;
}

// CHECK-LABEL: test_arguments_safe
// CHECK: void test_arguments_safe(const int* const arg1, const int& arg2{{[)]}}
void test_arguments_safe(int* arg1, int& arg2) {
    int val = *arg1 + arg2;
}

// CHECK-LABEL: test_arg_data_mutated
// CHECK: void test_arg_data_mutated(int* const arg{{[)]}}
void test_arg_data_mutated(int* arg) {
    *arg = 100;
}

// CHECK-LABEL: test_arg_ptr_mutated
// CHECK: void test_arg_ptr_mutated(const int* arg{{[)]}}
void test_arg_ptr_mutated(int* arg) {
    arg++;
}

// CHECK-LABEL: test_arg_ref_mutated
// CHECK: void test_arg_ref_mutated(int& arg{{[)]}}
// CHECK-NOT: const int{{&}} arg
void test_arg_ref_mutated(int& arg) {
    arg = 200;
}

// CHECK-LABEL: test_5_stars_safe
// CHECK: const int***** const p {{=}}
void test_5_stars_safe() {
    int a = 1;
    int* p1 = &a; int** p2 = &p1; int*** p3 = &p2; int**** p4 = &p3;
    int***** p = &p4;
    int b = *****p;
}

// CHECK-LABEL: test_5_stars_data_mutated
// CHECK: int***** const p {{=}}
// CHECK-NOT: const int{{[*]+}} const p
void test_5_stars_data_mutated() {
    int a = 1;
    int* p1 = &a; int** p2 = &p1; int*** p3 = &p2; int**** p4 = &p3;
    int***** p = &p4;
    *****p = 500;
}

// CHECK-LABEL: test_5_stars_ptr_mutated
// CHECK: const int***** p {{=}}
void test_5_stars_ptr_mutated() {
    int a = 1;
    int* p1 = &a; int** p2 = &p1; int*** p3 = &p2; int**** p4 = &p3;
    int***** p = &p4;
    p++;
}