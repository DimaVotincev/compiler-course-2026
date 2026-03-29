// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/kiselev_i_first_lab_ClangAST%pluginext -plugin override_check -fsyntax-only -verify %t/with_warnings.cpp
// RUN: %clang_cc1 -load %llvmshlibdir/kiselev_i_first_lab_ClangAST%pluginext -plugin override_check -fsyntax-only -verify %t/without_warnings.cpp

//--- with_warnings.cpp

class Base1 {
public:
  virtual void foo();
};

class Derived1 : public Base1 {
public:
  void foo() {} // expected-warning {{method 'foo' overrides base method but is not marked 'override'}}
};

class A1 {
public:
  virtual int sum(int a, int b);
};

class B1 : public A1 {
public:
  int sum(int a, int b) { return a + b; } // expected-warning {{method 'sum' overrides base method but is not marked 'override'}}
};

class A2 {
public:
  virtual void foo();
};

class B2 : public A2 {
public:
  void foo() {} // expected-warning {{method 'foo' overrides base method but is not marked 'override'}}
};

class C2 : public B2 {
public:
  void foo() {} // expected-warning {{method 'foo' overrides base method but is not marked 'override'}}
};

class L1 {
public:
  virtual void f();
};

class L2 : public L1 {};

class L3 : public L2 {
public:
  void f() {} // expected-warning {{method 'f' overrides base method but is not marked 'override'}}
};

class A {
public:
  virtual void f();
};

class B {
public:
  virtual void g();
};

class C : public A, public B {
public:
  void f() {} // expected-warning {{method 'f' overrides base method but is not marked 'override'}}
  void g() {} // expected-warning {{method 'g' overrides base method but is not marked 'override'}}
};

class Abstract {
public:
  virtual void run() = 0;
};

class Impl : public Abstract {
public:
  void run() {} // expected-warning {{method 'run' overrides base method but is not marked 'override'}}
};


//--- without_warnings.cpp

// expected-no-diagnostics

class Base2 {
public:
  virtual void foo();
};

class Derived2 : public Base2 {
public:
  void foo() override {}
};