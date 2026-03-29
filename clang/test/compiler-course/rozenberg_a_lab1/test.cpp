// RUN: %clang_cc1 -load %llvmshlibdir/rozenberg_a_lab1_ClangAST%pluginext -plugin rozenberg_a_plugin_1 -fsyntax-only %s 2>&1 | FileCheck %s

// TEST 1: FUNCTION ARGUMENTS (READ ONLY)
// CHECK-LABEL: test_params_const
// CHECK: void test_params_const(const int{{ }}*p, const int &r)
void test_params_const(int *p, int &r) {
    int x = *p;
    int y = r;
}

// TEST 2: LOCAL VARIABLES (READ ONLY)
// CHECK-LABEL: test_local_const
// CHECK: const int{{ }}*ptr = &val;
void test_local_const() {
    int val = 1;
    int *ptr = &val;
    int read = *ptr;
}

// TEST 3: DATA MUTATION (NO CONST)
// CHECK-LABEL: test_data_mut
// CHECK: int{{ }}*ptr = &val;
// CHECK-NOT: const{{ }}int *ptr
void test_data_mut() {
    int val = 1;
    int *ptr = &val;
    *ptr = 10;
}

// TEST 4: POINTER INCREMENT (DATA UNCHANGED -> SHOULD BE CONST)
// CHECK-LABEL: test_ptr_inc
// CHECK: const int{{ }}*ptr = &val;
void test_ptr_inc() {
    int val = 1;
    int *ptr = &val;
    ptr++; 
}

// TEST 5: ARRAY ACCESS (READ ONLY)
// CHECK-LABEL: test_array_read
// CHECK: const int{{ }}*p = arr;
void test_array_read() {
    int arr[] = {1, 2, 3};
    int *p = arr;
    int x = p[1];
}

// TEST 6: ARRAY ELEMENT MUTATION (NO CONST)
// CHECK-LABEL: test_array_mut
// CHECK: int{{ }}*p = arr;
// CHECK-NOT: const{{ }}int *p
void test_array_mut() {
    int arr[] = {1, 2, 3};
    int *p = arr;
    p[0] = 5;
}

// TEST 7: REFERENCE READ ONLY
// CHECK-LABEL: test_ref_const
// CHECK: const int{{ }}&ref = a;
void test_ref_const() {
    int a = 10;
    int &ref = a;
    int b = ref;
}

// TEST 8: REFERENCE MUTATION (NO CONST)
// CHECK-LABEL: test_ref_mut
// CHECK: int{{ }}&ref = a;
// CHECK-NOT: const{{ }}int &ref
void test_ref_mut() {
    int a = 10;
    int &ref = a;
    ref++;
}

// TEST 9: ALREADY CONST (NO CHANGES)
// CHECK-LABEL: test_already_const
// CHECK: const int{{ }}*ptr = &val;
void test_already_const() {
    int val = 5;
    const int *ptr = &val;
    int x = *ptr;
}

// TEST 10: BRACKETS USAGE
// CHECK-LABEL: test_brackets
// CHECK: const int{{ }}*ptr = &val;
void test_brackets() {
    int val = 1;
    int *ptr = &val;
    (*ptr);
}

// TEST 11: DOUBLE POINTER - DATA READ ONLY
// CHECK-LABEL: test_double_ptr_read
// CHECK: const int{{ }}**ptr = &p1;
void test_double_ptr_read() {
    int val = 10;
    int *p1 = &val;
    int **ptr = &p1;
    int x = **ptr;
}

// TEST 12: DOUBLE POINTER - DATA MUTATED (NO CONST)
// CHECK-LABEL: test_double_ptr_mut
// CHECK: int{{ }}**ptr = &p1;
// CHECK-NOT: const{{ }}int **ptr
void test_double_ptr_mut() {
    int val = 10;
    int *p1 = &val;
    int **ptr = &p1;
    **ptr = 20;
}

// TEST 13: TRIPLE POINTER - DATA READ ONLY
// CHECK-LABEL: test_triple_ptr_read
// CHECK: const int{{ }}***ptr = &p2;
void test_triple_ptr_read() {
    int val = 5;
    int *p1 = &val; int **p2 = &p1;
    int ***ptr = &p2;
    int x = ***ptr;
}

// TEST 14: MIXED ARRAY AND POINTER DEREFERENCE
// CHECK-LABEL: test_mixed_deref
// CHECK: const int{{ }}**ptr = &p1;
void test_mixed_deref() {
    int val = 7;
    int *p1 = &val;
    int **ptr = &p1;
    int x = ptr[0][0];
}

// TEST 15: MULTI-LEVEL WITH INCREMENT (NO CONST)
// CHECK-LABEL: test_multi_inc_mut
// CHECK: int{{ }}***ptr = &p2;
// CHECK-NOT: const{{ }}int ***ptr
void test_multi_inc_mut() {
    int val = 1;
    int *p1 = &val; int **p2 = &p1;
    int ***ptr = &p2;
    (***ptr)++;
}