// RUN: %clang_cc1 -fcxx-exceptions -fexceptions -load %llvmshlibdir/lobanov_d_1lab_ClangAST.so -add-plugin add-noexcept -ast-dump %s 2>&1 | FileCheck %s

// CHECK: FunctionDecl {{.*}} normalFunction 'void () noexcept'
void normalFunction() {
  int x = 10;
  int y = 20;
  int z = x + y;
}

// CHECK: FunctionDecl {{.*}} throwing_function 'void ()'{{.*}}
// CHECK-NOT: noexcept
void throwing_function() { throw 42; }

// CHECK: FunctionDecl {{.*}} callThrow 'void ()'
void callThrow() { throw 1; }

// CHECK: FunctionDecl {{.*}} calculateDivision 'double (double, double)'{{.*}}
// CHECK-NOT: noexcept
double calculateDivision(double a, double b) {
  if (b == 0) {
    throw "Division by zero!";
  }
  return a / b;
}

// CHECK: FunctionDecl {{.*}} emptyFunction 'void () noexcept'
void emptyFunction() {
  // empty
}

// CHECK: FunctionDecl {{.*}} conditionalFunction 'void (bool)'{{.*}}
// CHECK-NOT: noexcept
void conditionalFunction(bool flag) {
  if (flag) {
    throw 42;
  }
}

// CHECK: FunctionDecl {{.*}} nestedCalls 'void ()'{{.*}}
// CHECK-NOT: noexcept
void nestedCalls() {
  throwing_function(); // calls function that throws
}

// CHECK: FunctionDecl {{.*}} recursiveFunction 'void (int) noexcept'
void recursiveFunction(int n) {
  if (n > 0) {
    recursiveFunction(n - 1);
  }
}