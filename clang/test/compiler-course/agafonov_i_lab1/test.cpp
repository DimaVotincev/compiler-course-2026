// RUN: %clang_cc1 -load %llvmshlibdir/agafonov_i_lab1_ClangAST%pluginext -plugin constify_plugin -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK-LABEL: void test_ref_read(const int &r)
void test_ref_read(int &r) {
    int x = r;
}

// CHECK-LABEL: void test_ref_write(int &r)
void test_ref_write(int &r) {
    r = 10;
}

// CHECK-LABEL: void test_ptr_static(const int * const p)
void test_ptr_static(int *p) {
    int x = *p;
}

// CHECK-LABEL: void test_ptr_addr_only(const int * p)
void test_ptr_addr_only(int *p) {
    int x = 10;
    p = &x;
}

// CHECK-LABEL: void test_ptr_data_only(int * const p)
void test_ptr_data_only(int *p) {
    *p = 100;
}

// CHECK-LABEL: void test_ptr_ptr(const int ** pp)
void test_ptr_ptr(int **pp) {
    int x = **pp;
}

// CHECK-LABEL: void test_array_mutation(int * const arr)
void test_array_mutation(int *arr) {
    arr[5] = 42;
}

// CHECK-LABEL: void test_array_read(const int (*parr)[10])
void test_array_read(int (*parr)[10]) {
    int x = (*parr)[0];
}

// CHECK-LABEL: void test_inc_data(int * const p)
void test_inc_data(int *p) {
    (*p)++;
}

// CHECK-LABEL: void test_inc_ptr(int * p)
void test_inc_ptr(int *p) {
    p++;
}

// CHECK-LABEL: void test_local_var()
void test_local_var() {
    int x = 0;
    // CHECK: const int * const lp = &x;
    int *lp = &x;
    int y = *lp;
}

// CHECK-LABEL: void test_compound(int * const p)
void test_compound(int *p) {
    *p += 5;
}


// CHECK-LABEL: void test_already_const(const int * const p)
void test_already_const(const int * const p) {
    int x = *p;
}

// CHECK-LABEL: void test_spaces(const int * const p)
void test_spaces(int   * p) {
    int x = *p;
}