// RUN: %clang_cc1 -load %llvmshlibdir/akimov_i_lab1_ClangAST%pluginext -plugin constify_plugin -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK-LABEL: fully_unmodified
// CHECK: const int* const ptr {{=}}
void fully_unmodified() {
    int val = 42;
    int* ptr = &val;
    int x = *ptr;
}

// CHECK-LABEL: pointer_modified
// CHECK: const int* ptr {{=}}
void pointer_modified() {
    int val = 42;
    int* ptr = &val;
    ptr++;
}

// CHECK-LABEL: pointee_modified
// CHECK: int* const ptr {{=}}
void pointee_modified() {
    int val = 42;
    int* ptr = &val;
    *ptr = 100;
}

// CHECK-LABEL: both_modified
// CHECK: int* ptr {{=}}
void both_modified() {
    int val = 42;
    int* ptr = &val;
    *ptr = 100;
    ptr = nullptr;
}

// CHECK-LABEL: ref_unmodified
// CHECK: const int& ref {{=}}
void ref_unmodified() {
    int val = 42;
    int& ref = val;
    int x = ref;
}

// CHECK-LABEL: ref_modified
// CHECK: int& ref {{=}}
// CHECK-NOT: const int& ref
void ref_modified() {
    int val = 42;
    int& ref = val;
    ref = 99;
}


// CHECK-LABEL: param_test
// CHECK: void param_test(const int* const p, const int& r)
void param_test(int* p, int& r) {
    int x = *p + r;
}

// CHECK-LABEL: param_mixed
// CHECK: void param_mixed(int* const p, const int* q)
void param_mixed(int* p, int* q) {
    *p = 5;
    q++;
}

// CHECK-LABEL: double_ptr
// CHECK: const int** const dp {{=}}
void double_ptr() {
    int val = 1;
    int* p = &val;
    int** dp = &p;
    int x = **dp;
}

struct S { int a; };
// CHECK-LABEL: member_access
// CHECK: S* const s {{=}}
void member_access() {
    S obj{10};
    S* s = &obj;
    s->a = 20;
}

// CHECK-LABEL: array_access
// CHECK: int* const arr {{=}}
void array_access() {
    int data[3] = {1,2,3};
    int* arr = data;
    arr[1] = 42;
}
