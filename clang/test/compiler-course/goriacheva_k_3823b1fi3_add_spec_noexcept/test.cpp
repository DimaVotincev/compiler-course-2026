// RUN: %clang_cc1 -fcxx-exceptions -load %llvmshlibdir/goriacheva_k_3823b1fi3_add_spec_noexcept_ClangAST%pluginext -plugin spec_noexcept -fsyntax-only %s 2>&1 | FileCheck %s

// =======================
// Test 1: simple safe function
// =======================

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:[[@LINE+1]]:5 safe 'int () noexcept'
int safe() {
  return 42;
}

// =======================
// Test 2: function that throws
// =======================

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:[[@LINE+1]]:5 danger 'int ()'
int danger() {
  throw 1;
}

// =======================
// Test 3: call to noexcept function
// =======================

int callee() noexcept {
  return 1;
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:[[@LINE+1]]:5 callerSafe 'int () noexcept'
int callerSafe() {
  return callee();
}

// =======================
// Test 4: call to potentially throwing function
// =======================

int throwingCallee() {
  throw 2;
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:[[@LINE+1]]:5 callerDanger 'int ()'
int callerDanger() {
  return throwingCallee();
}

// =======================
// Test 5: new expression (may throw)
// =======================

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:[[@LINE+1]]:6 allocate 'int *()'
int* allocate() {
  return new int(10);
}

// =======================
// Test 6: already noexcept
// =======================

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:[[@LINE+1]]:5 alreadyNoexcept 'int () noexcept'
int alreadyNoexcept() noexcept {
  return 5;
}

