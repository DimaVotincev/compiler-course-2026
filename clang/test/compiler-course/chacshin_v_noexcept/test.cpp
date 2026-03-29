// RUN: %clang_cc1 -fcxx-exceptions -fexceptions -load %llvmshlibdir/libChacshinNoexcept_Chacshin_Vladimir_FIIT3_ClangAST%pluginext -add-plugin chacshin_noexcept_plugin -ast-dump %s | FileCheck %s

class Base {
public:
    virtual ~Base() {}
};

class Derived : public Base {};

struct CtorThrow {
    CtorThrow() { throw 1; }
};

struct CtorNoThrow {
    CtorNoThrow() {}
};

// CHECK-LABEL: FunctionDecl {{.*}} empty 'void () noexcept'
void empty() {}

// CHECK-LABEL: FunctionDecl {{.*}} simpleThrow 'void ()'
void simpleThrow() {
    throw 42;
}

// CHECK-LABEL: FunctionDecl {{.*}} withNew 'void ()'
void withNew() {
    int* p = new int;
    delete p;
}

// CHECK-LABEL: FunctionDecl {{.*}} castToRef 'void ()'
void castToRef() {
    Derived d;
    Base& b = d;
    Derived& d2 = dynamic_cast<Derived&>(b);
}

// CHECK-LABEL: FunctionDecl {{.*}} castToPtr 'void () noexcept'
void castToPtr() {
    Derived d;
    Base* b = &d;
    Derived* d2 = dynamic_cast<Derived*>(b);
}

// CHECK-LABEL: FunctionDecl {{.*}} callNoThrow 'void () noexcept'
void callNoThrow() {
    empty();
}

// CHECK-LABEL: FunctionDecl {{.*}} callThrow 'void ()'
void callThrow() {
    simpleThrow();
}

// CHECK-LABEL: FunctionDecl {{.*}} fact 'int (int) noexcept'
int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}

// CHECK-LABEL: FunctionDecl {{.*}} factThrow 'int (int)'
int factThrow(int n) {
    if (n <= 1) throw 1;
    return n * factThrow(n - 1);
}

// CHECK-LABEL: FunctionDecl {{.*}} lambdaThrow 'void ()'
void lambdaThrow() {
    auto l = []() { throw 1; };
    l();
}

// CHECK-LABEL: FunctionDecl {{.*}} lambdaNoThrow 'void () noexcept'
void lambdaNoThrow() {
    auto l = []() {};
    l();
}

// CHECK-LABEL: FunctionDecl {{.*}} createCtorThrow 'void ()'
void createCtorThrow() {
    CtorThrow obj;
}

// CHECK-LABEL: FunctionDecl {{.*}} createCtorNoThrow 'void () noexcept'
void createCtorNoThrow() {
    CtorNoThrow obj;
}

// CHECK-LABEL: FunctionDecl {{.*}} callCtorThrow 'void ()'
void callCtorThrow() {
    CtorThrow obj;
}

// CHECK-LABEL: FunctionDecl {{.*}} callCtorNoThrow 'void () noexcept'
void callCtorNoThrow() {
    CtorNoThrow obj;
}

void foo(int) {}

// CHECK-LABEL: FunctionDecl {{.*}} pointerCall 'void (void (*)(int))'
void pointerCall(void (*f)(int)) {
    f(42);
}