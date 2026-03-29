// RUN: %clang_cc1 -fcxx-exceptions -fexceptions -load %llvmshlibdir/gutyansky_a_lab1_ClangAST%pluginext -plugin gutyansky_a_ast_noexcept_plugin -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> col:6 empty 'void () noexcept'
void empty() {}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> col:6 emptyNoExcept 'void () noexcept'
void emptyNoExcept() noexcept {}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> col:5 used simpleNoThrow 'int () noexcept'
int simpleNoThrow() { return -1; }

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:13:6 simpleThrow 'void ()'
void simpleThrow() { 
    throw "Exception";
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:18:6 used conditionalThrow 'void (bool)'
void conditionalThrow(bool cond) {
    if (cond) {
        throw "EEEE";
    }
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:25:5 simpleCall 'int () noexcept'
int simpleCall() {
    return simpleNoThrow();
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:30:6 simpleCallThrow 'void ()'
void simpleCallThrow() {
    conditionalThrow(false);
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:35:6 lambdaThrow 'void ()'
void lambdaThrow() {
    auto f = []() { throw "aaaa"; };
    f();
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:41:6 lambdaNoThrow 'void () noexcept'
void lambdaNoThrow() {
    auto f = []() { int b = 2 + 3; };
    f();
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:47:6 newThrow 'void ()'
void newThrow() {
    int* a = new int;
    delete a;
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:53:5 used fact 'int (int) noexcept'
int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:59:5 recursiveThrow 'int (int)'
int recursiveThrow(int n) {
    if (n <= 1) throw 1;
    return n * fact(n - 1);
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:65:6 pointerCall 'void (void (*)(int))'
void pointerCall(void (*f)(int)) {
    f(123);
}

class Bar {
public:
    Bar() { throw 0; }
    Bar(int x) {}

    void methodNoThrow() {}
    void methodThrow() { throw 42; }

    virtual void methodVirtualNoThrow() {}
};

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:81:6 constructNoThrow 'void () noexcept'
void constructNoThrow() {
    Bar b(111);
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:86:6 constructThrow 'void ()'
void constructThrow() {
    Bar b;
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:91:6 callMethodNoThrow 'void () noexcept'
void callMethodNoThrow() {
    Bar b(111);
    b.methodNoThrow();
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:97:6 callMethodThrow 'void ()'
void callMethodThrow() {
    Bar b(111);
    b.methodThrow();
}

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> line:103:6 callMethodVirtual 'void ()'
void callMethodVirtual() {
    Bar b(111);
    b.methodVirtualNoThrow();
}