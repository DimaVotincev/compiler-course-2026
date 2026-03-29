// RUN: %clang_cc1 -load %llvmshlibdir/sakharov_a_lab_1_ClangAST%pluginext -plugin sakharov_add_const -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK: void basic_pointers_and_refs() {
// CHECK-NEXT:     int val = 42;
// CHECK-NEXT:     const int &ref_const = val;
// CHECK-NEXT:     const int *const ptr_const = &val;
// CHECK-NEXT:     int &ref_mut = val;
// CHECK-NEXT:     ref_mut = 10;
// CHECK-NEXT:     int *ptr_mut_value = &val;
// CHECK-NEXT:     *ptr_mut_value = 20;
// CHECK-NEXT:     int *ptr_mut_addr = &val;
// CHECK-NEXT:     int other = 5;
// CHECK-NEXT:     ptr_mut_addr = &other;
// CHECK-NEXT: }

void basic_pointers_and_refs() {
    int val = 42;
    int& ref_const = val; 
    int* ptr_const = &val;
    int& ref_mut = val;
    ref_mut = 10;
    int* ptr_mut_value = &val;
    *ptr_mut_value = 20;
    int* ptr_mut_addr = &val;
    int other = 5;
    ptr_mut_addr = &other;
}

// CHECK: void function_params(const int &p_ref_const, const int *const p_ptr_const, int &p_ref_mut, int *p_ptr_mut) {
// CHECK-NEXT:     const int &local_ref = p_ref_const;
// CHECK-NEXT:     p_ref_mut += 5;
// CHECK-NEXT:     p_ptr_mut++;
// CHECK-NEXT: }

void function_params(int& p_ref_const, int* p_ptr_const, int& p_ref_mut, int* p_ptr_mut) {
    int& local_ref = p_ref_const;
    p_ref_mut += 5;
    p_ptr_mut++;
}

// CHECK: void unary_and_binary_ops() {
// CHECK-NEXT:     int arr[5] = {1, 2, 3, 4, 5};
// CHECK-NEXT:     int *ptr_arr_mut = arr;
// CHECK-NEXT:     ptr_arr_mut[2] = 10;
// CHECK-NEXT:     int *ptr_inc = arr;
// CHECK-NEXT:     ptr_inc++;
// CHECK-NEXT:     int *ptr_dec = arr;
// CHECK-NEXT:     --ptr_dec;
// CHECK-NEXT:     const int *const ptr_arr_const = arr;
// CHECK-NEXT:     int x = ptr_arr_const[1];
// CHECK-NEXT: }

void unary_and_binary_ops() {
    int arr[5] = {1, 2, 3, 4, 5};
    int* ptr_arr_mut = arr;
    ptr_arr_mut[2] = 10;
    int* ptr_inc = arr;
    ptr_inc++;
    int* ptr_dec = arr;
    --ptr_dec;
    int* ptr_arr_const = arr;
    int x = ptr_arr_const[1];
}

// CHECK: void multiple_indirection() {
// CHECK-NEXT:     int x = 10;
// CHECK-NEXT:     const int *const px = &x;
// CHECK-NEXT:     const int **const ppx = &px;
// CHECK-NEXT:     const int ***const pppx = &ppx;
// CHECK-NEXT: }

void multiple_indirection() {
    int x = 10;
    int* px = &x;
    int** ppx = &px;
    int*** pppx = &ppx;
}

// CHECK: void already_const_variables() {
// CHECK-NEXT:     int a = 5;
// CHECK-NEXT:     const int &r1 = a;
// CHECK-NEXT:     const int *const p1 = &a;
// CHECK-NEXT: }

void already_const_variables() {
    int a = 5;
    const int& r1 = a;
    const int* const p1 = &a;
}

// CHECK: void aliasing_test_no_direct_mutation() {
// CHECK-NEXT:     int *y = new int(10);
// CHECK-NEXT:     *y = 25;
// CHECK-NEXT:     const int *const second_y = y;
// CHECK-NEXT: }

void aliasing_test_no_direct_mutation() {
    int* y = new int(10);
    *y = 25;
    int* second_y = y; 
}
