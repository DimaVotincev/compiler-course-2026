// RUN: split-file %s %t
// RUN: %clang_cc1 -std=c++17 -load %llvmshlibdir/telnov_a_link_indicator_ClangAST%pluginext -plugin example_plugin -fsyntax-only %t/with_const.cpp 2>&1 | FileCheck %s --check-prefix=CHECK-WITH
// RUN: %clang_cc1 -std=c++17 -load %llvmshlibdir/telnov_a_link_indicator_ClangAST%pluginext -plugin example_plugin -fsyntax-only %t/without_const.cpp 2>&1 | FileCheck %s --check-prefix=CHECK-WITHOUT

// CHECK-WITH: void ref_read_only(const int& value)
// CHECK-WITH: void ptr_read_only(const int* const ptr)
// CHECK-WITH: void ptr_pointee_modified(int* const ptr)
// CHECK-WITH: void ptr_reassigned(int* ptr)
// CHECK-WITH: const int& ref = value;
// CHECK-WITH: const int* const ptr = &value;
// CHECK-WITH: void pass_ref_to_const(const int& value)
// CHECK-WITH: void pass_ptr_to_const(const int* const ptr)

//--- with_const.cpp
void takes_const_ref(const int&);
void takes_mut_ref(int&);
void takes_const_ptr(const int*);
void takes_mut_ptr(int*);

void ref_read_only(int& value) {
  int x = value;
  (void)x;
}

void ptr_read_only(int* ptr) {
  int x = *ptr;
  (void)x;
}

void ptr_pointee_modified(int* ptr) {
  *ptr = 42;
}

void ptr_reassigned(int* ptr) {
  int local = 0;
  ptr = &local;
}

void local_ref_candidate() {
  int value = 0;
  int& ref = value;
  int x = ref;
  (void)x;
}

void local_ptr_candidate() {
  int value = 0;
  int* ptr = &value;
  int x = *ptr;
  (void)x;
}

void pass_ref_to_const(int& value) {
  takes_const_ref(value);
}

void pass_ptr_to_const(int* ptr) {
  takes_const_ptr(ptr);
}

// CHECK-WITHOUT: void ref_modified(int& value)
// CHECK-WITHOUT: void ptr_increment(int* ptr)
// CHECK-WITHOUT: void pass_ref_to_mut(int& value)
// CHECK-WITHOUT: void pass_ptr_to_mut(int* ptr)
// CHECK-WITHOUT: void already_const_ref(const int& value)
// CHECK-WITHOUT: void already_const_ptr(const int* const ptr)

//--- without_const.cpp
void takes_const_ref(const int&);
void takes_mut_ref(int&);
void takes_const_ptr(const int*);
void takes_mut_ptr(int*);

void ref_modified(int& value) {
  value = 10;
}

void ptr_increment(int* ptr) {
  ++ptr;
}

void pass_ref_to_mut(int& value) {
  takes_mut_ref(value);
}

void pass_ptr_to_mut(int* ptr) {
  takes_mut_ptr(ptr);
}

void already_const_ref(const int& value) {
  int x = value;
  (void)x;
}

void already_const_ptr(const int* const ptr) {
  int x = *ptr;
  (void)x;
}