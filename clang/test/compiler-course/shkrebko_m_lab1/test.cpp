// RUN: %clang_cc1 -load %llvmshlibdir/shkrebko_m_lab1_ClangAST%pluginext -plugin const_plugin -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK-LABEL: test_ptr_none
// CHECK: const int* const p {{=}}
void test_ptr_none() {
    int x = 10;
    int* p = &x;
    int y = *p;
}


// CHECK-LABEL: test_ptr_data
// CHECK: int* const p {{=}}
void test_ptr_data() {
    int z = 0;
    int* p = &z;
    *p = 123;
}

// CHECK-LABEL: test_ptr_both
// CHECK: int* p {{=}}
void test_ptr_both() {
    int arr[2] = {7, 8};
    int* p = arr;
    p++;
    *p = 9;
}

// CHECK-LABEL: test_ref_read
// CHECK: const double& r {{=}}
void test_ref_read() {
    double val = 3.14;
    double& r = val;
    double x = r;
}

// CHECK-LABEL: test_ref_write
// CHECK: double& r {{=}}
void test_ref_write() {
    double data = 1.0;
    double& r = data;
    r = 2.0;
}

// CHECK-LABEL: test_const_qual
// CHECK: const int* const p {{=}}
void test_const_qual() {
    int num = 42;
    const int* const p = &num;
    int cpy = *p;
}

// CHECK-LABEL: test_array_index
// CHECK: int* const ptr {{=}}
void test_array_index() {
    int arr[5] = {0};
    int* ptr = arr;
    ptr[4] = 55;
}

// CHECK-LABEL: test_params_safe
// CHECK: void test_params_safe(const int* const p, const double& r)
void test_params_safe(int* p, double& r) {
    double res = *p + r;
}

// CHECK-LABEL: test_params_data
// CHECK: void test_params_data(int* const p)
void test_params_data(int* p) {
    *p = 111;
}

// CHECK-LABEL: test_params_ptr
// CHECK: void test_params_ptr(const int* p)
void test_params_ptr(int* p) {
    p++;
}

// CHECK-LABEL: test_params_ref
// CHECK: void test_params_ref(double& r)
void test_params_ref(double& r) {
    r = 3.14;
}

// CHECK-LABEL: test_stars3_none
// CHECK: const int*** const p {{=}}
void test_stars3_none() {
    int x = 5;
    int* p1 = &x;
    int** p2 = &p1;
    int*** p = &p2;
    int y = ***p;
}

// CHECK-LABEL: test_stars3_data
// CHECK: int*** const p {{=}}
void test_stars3_data() {
    int x = 5;
    int* p1 = &x;
    int** p2 = &p1;
    int*** p = &p2;
    ***p = 10;
}

// CHECK-LABEL: test_stars3_ptr
// CHECK: const int*** p {{=}}
void test_stars3_ptr() {
    int x = 5;
    int* p1 = &x;
    int** p2 = &p1;
    int*** p = &p2;
    p++;
}