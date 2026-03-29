// RUN: %clang_cc1 -load %llvmshlibdir/ovsyannikov_n_lab1_ClangAST%pluginext -plugin ovsyannikov_no_override -Wno-inconsistent-missing-override -fsyntax-only -verify %s

class Base {
public:
    virtual void func1() {}
    virtual void func2(int x) {}
    virtual ~Base() {}
};

class Interface {
public:
    virtual void action() = 0;
};

class Derived : public Base {
public:
    void func1() {} // expected-warning {{ovsyannikov-check: method 'func1' overrides a virtual function but lacks 'override' specifier}}

    void func2(int x) override {}

    ~Derived() {} // expected-warning {{ovsyannikov-check: method '~Derived' overrides a virtual function but lacks 'override' specifier}}
};

class GrandDerived : public Derived {
public:
    void func1() override {}

    void func2(int x) {} // expected-warning {{ovsyannikov-check: method 'func2' overrides a virtual function but lacks 'override' specifier}}
};

class MultiDerived : public Base, public Interface {
public:
    void action() {} // expected-warning {{ovsyannikov-check: method 'action' overrides a virtual function but lacks 'override' specifier}}

    void func1() override {}
};

class Independent {
public:
    void someMethod() {}

    virtual void startVirtual() {}
};

class Overloader : public Base {
public:
    void func2(double d) {}

    void myOwnMethod() {}
};

class FinalTest : public Base {
public:
    void func1() final {} // expected-warning {{ovsyannikov-check: method 'func1' overrides a virtual function but lacks 'override' specifier}}
};
