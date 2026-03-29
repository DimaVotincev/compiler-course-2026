// RUN: %clang_cc1 -fcxx-exceptions -fexceptions -load %llvmshlibdir/papulina_y_noexcept_func_ClangAST%pluginext -add-plugin papulina_y_noexcept_func_plugin -fsyntax-only %s 2>&1 | FileCheck %s
void goodFunc() {
  int x = 15;
}
void funcWithThrow() { throw 42; }

void funcWithThrowAndConditions() {
    int x = 15;
    if (x > 0) {
        throw x;
    }
}

void callFuncWithExcept() {
    funcWithThrow();
}

void callFuncWithoutExcept() {
    goodFunc();
}

void funcWithNew() {
  int * ptr = new int{15};
  delete ptr;
}

void level3() { throw 15; }
void level2() { level3(); }  
void level1() { level2(); }

void process() {
    funcWithThrow();
    funcWithThrow();
    funcWithThrow();
}

void recursiveWithThrow(int n) {
    if (n > 0) {
        if (n == 15) throw n; 
        recursiveWithThrow(n - 1);
    }
}

class TestClass {
public:
    TestClass() { throw 42; } 
};
void createObject() {
    TestClass obj; 
}

// CHECK: Function goodFunc: exception spec: 5
// CHECK: Function funcWithThrow: exception spec: 0
// CHECK: Function funcWithThrowAndConditions: exception spec: 0
// CHECK: Function callFuncWithExcept: exception spec: 0
// CHECK: Function callFuncWithoutExcept: exception spec: 5
// CHECK: Function funcWithNew: exception spec: 0
// CHECK: Function level3: exception spec: 0
// CHECK: Function level2: exception spec: 0
// CHECK: Function level1: exception spec: 0
// CHECK: Function process: exception spec: 0
// CHECK: Function recursiveWithThrow: exception spec: 0
// CHECK: Function TestClass: exception spec: 0
// CHECK: Function createObject: exception spec: 0