// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/OverrideVisitor_Guseva_Alena_FIIT2_ClangAST%pluginext -plugin OverrideCheckPlugin -Wno-inconsistent-missing-override -fsyntax-only -verify %t/with_warnings.cpp
// RUN: %clang_cc1 -load %llvmshlibdir/OverrideVisitor_Guseva_Alena_FIIT2_ClangAST%pluginext -plugin OverrideCheckPlugin -Wno-inconsistent-missing-override -fsyntax-only -verify %t/without_warnings.cpp

//--- with_warnings.cpp
class Base1 {
public:
    virtual void foo();
};

class Derived1 : public Base1 {
public:
    void foo(); // expected-warning {{virtual method is not marked 'override'}}
};
class Base2 {
public:
    virtual void foo();
};

class Mid : public Base2 {};

class Derived2 : public Mid {
public:
    void foo(); // expected-warning {{virtual method is not marked 'override'}}
};

class Base5 {
public:
    virtual void foo();
    virtual void bar();
};

class Derived5 : public Base5 {
public:
    void foo() { } // expected-warning {{virtual method is not marked 'override'}}
    void bar() override { }
};

//--- without_warnings.cpp
// expected-no-diagnostics

class Base3 {
public:
    virtual void foo();
};

class Derived3 : public Base3 {
public:
    void foo() override;
};

class Base4 {
public:
    void foo();
};

class Derived4 : public Base4 {
public:
    void foo();
};
