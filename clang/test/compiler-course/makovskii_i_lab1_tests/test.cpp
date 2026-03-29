// RUN: %clang_cc1 -load %llvmshlibdir/makovskii_i_lab1_ClangAST%pluginext -plugin auto_const_plugin -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK-LABEL: check_fully_unmodified
// CHECK: const int* const ptr {{=}}
void check_fully_unmodified() {
    int val = 42;
    int* ptr = &val;
    int read = *ptr;
}

// CHECK-LABEL: check_pointer_mutated
// CHECK: const int* ptr2 {{=}}
void check_pointer_mutated() {
    int val = 42;
    int* ptr2 = &val;
    ptr2 += 1;
}

// CHECK-LABEL: check_data_mutated
// CHECK: int* const ptr3 {{=}}
void check_data_mutated() {
    int val = 42;
    int* ptr3 = &val;
    *ptr3 = 100;
}

// CHECK-LABEL: check_reference_unmodified
// CHECK: const int& ref_val {{=}}
void check_reference_unmodified() {
    int val = 42;
    int& ref_val = val;
    int x = ref_val + 5;
}

// CHECK-LABEL: check_reference_mutated
// CHECK: int& ref_mut {{=}}
// CHECK-NOT: const int{{&}} ref_mut
void check_reference_mutated() {
    int val = 42;
    int& ref_mut = val;
    ref_mut = 99;
}

// CHECK-LABEL: check_arrays_mutation
// CHECK: int* const arr_ptr {{=}}
void check_arrays_mutation() {
    int buffer[3] = {10, 20, 30};
    int* arr_ptr = buffer;
    arr_ptr[1] = 50; 
}

// CHECK-LABEL: check_function_params
// CHECK: void check_function_params(const int* const param_ptr, const int& param_ref{{[)]}}
void check_function_params(int* param_ptr, int& param_ref) {
    int sum = *param_ptr + param_ref;
}

// CHECK-LABEL: check_function_params_mutated
// CHECK: void check_function_params_mutated(int* const param1, const int* param2{{[)]}}
void check_function_params_mutated(int* param1, int* param2) {
    *param1 = 5;
    param2++;
}

// CHECK-LABEL: check_double_pointers
// CHECK: const int** const d_ptr {{=}}
void check_double_pointers() {
    int val = 1;
    int* p1 = &val;
    int** d_ptr = &p1;
    int read = **d_ptr;
}
