// RUN: %clang_cc1 -std=c++17 -fcxx-exceptions -fexceptions -load %llvmshlibdir/SavvaDariyaPlugin_Savva_Dariya_FIIT1_ClangAST%pluginext -plugin savva_dariya_3823b1fi1 -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK: FunctionDecl {{.*}} simpleSafe 'void () noexcept'
void simpleSafe() {
    int a = 2 + 2;
}

// CHECK: FunctionDecl {{.*}} simpleThrow 'void ()'
void simpleThrow() {
    throw 1;
}

// CHECK: FunctionDecl {{.*}} callThrow 'void ()'
void callThrow() {
    simpleThrow();
}

// CHECK: FunctionDecl {{.*}} useNew 'void ()'
void useNew() {
    int* ptr = new int(5);
}

// CHECK: FunctionDecl {{.*}} recursiveSafe 'void (int) noexcept'
void recursiveSafe(int n) {
    if (n > 0) recursiveSafe(n - 1);
}

// CHECK: FunctionDecl {{.*}} ping 'void ()'
// CHECK: FunctionDecl {{.*}} pong 'void ()'

void pong();
void ping() {
    pong();
}
void pong() {
    ping();
    throw 1;
}

// CHECK: FunctionDecl {{.*}} alreadyNoexcept 'void () noexcept'
void alreadyNoexcept() noexcept {
    int x = 0;
}

void level3() { throw 42; }
void level2() { level3(); }
void level1() { level2(); }

// CHECK-DAG: FunctionDecl {{.*}} level1{{.*}} 'void ()'
// CHECK-DAG: FunctionDecl {{.*}} level2{{.*}} 'void ()'
// CHECK-DAG: FunctionDecl {{.*}} level3{{.*}} 'void ()'

// CHECK: FunctionDecl {{.*}} conditionalThrow 'void (int)'
void conditionalThrow(int x) {
    if (x > 0) {
        throw x;
    }
}

