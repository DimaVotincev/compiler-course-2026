// RUN: %clang_cc1 -fcxx-exceptions -fexceptions -load %llvmshlibdir/nikitina_v_lab1_ClangAST%pluginext -add-plugin nikitina_v_noexcept_plugin -ast-dump %s | FileCheck %s

void SafeCalculation() {
    double a = 3.14;
    double b = 2.0;
    double c = a * b;
}

void RaisesException() { 
    throw "Error occurred!"; 
}

void ConditionalThrow(bool flag) {
    int value = 100;
    if (flag) {
        throw value;
    }
}

void WrapperForThrow() {
    RaisesException();
}

void WrapperForSafe() {
    SafeCalculation();
}

void HeapAllocation() {
    long* data = new long[100];
    delete[] data;
}

void DeepCallC() { throw 404; }
void DeepCallB() { DeepCallC(); }  
void DeepCallA() { DeepCallB(); }

void ExecuteTask() {
    RaisesException();
    RaisesException();
}

void FibonacciThrow(int step) {
    if (step > 0) {
        if (step == 42) throw step; 
        FibonacciThrow(step - 1);
    }
}

class DangerStruct {
public:
    DangerStruct() { throw 1; } 
};

void InstantiateStruct() {
    DangerStruct instance; 
}


// CHECK: FunctionDecl {{.*}} SafeCalculation 'void () noexcept'
// CHECK: FunctionDecl {{.*}} RaisesException 'void ()'
// CHECK: FunctionDecl {{.*}} ConditionalThrow 'void (bool)'
// CHECK: FunctionDecl {{.*}} WrapperForThrow 'void ()'
// CHECK: FunctionDecl {{.*}} WrapperForSafe 'void () noexcept'
// CHECK: FunctionDecl {{.*}} HeapAllocation 'void ()'
// CHECK: FunctionDecl {{.*}} DeepCallC 'void ()'
// CHECK: FunctionDecl {{.*}} DeepCallB 'void ()'
// CHECK: FunctionDecl {{.*}} DeepCallA 'void ()'
// CHECK: FunctionDecl {{.*}} ExecuteTask 'void ()'
// CHECK: FunctionDecl {{.*}} FibonacciThrow 'void (int)'
// CHECK: CXXConstructorDecl {{.*}} DangerStruct 'void ()'
// CHECK: FunctionDecl {{.*}} InstantiateStruct 'void ()'