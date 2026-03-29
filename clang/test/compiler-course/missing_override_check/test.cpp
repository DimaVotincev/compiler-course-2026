// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/missing_override_check_ClangAST%pluginext -plugin override_check -fsyntax-only -verify %t/with_warnings.cpp
// RUN: %clang_cc1 -load %llvmshlibdir/missing_override_check_ClangAST%pluginext -plugin override_check -fsyntax-only -verify %t/without_warnings.cpp

//--- with_warnings.cpp

class Base1 {
public:
  virtual void foo();
};

class Derived1 : public Base1 {
public:
  void foo() {} // expected-warning {{method 'foo' overrides base method but is not marked 'override'}}
};

class Base2 {
public:
  virtual int sum(int a, int b);
};

class Derived2 : public Base2 {
public:
  int sum(int a, int b) { return a + b; } // expected-warning {{method 'sum' overrides base method but is not marked 'override'}}
};

class Left {
public:
  virtual void left();
};

class Right {
public:
  virtual void right();
};

class Child : public Left, public Right {
public:
  void left() {} // expected-warning {{method 'left' overrides base method but is not marked 'override'}}
  void right() {} // expected-warning {{method 'right' overrides base method but is not marked 'override'}}
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

class Base3 {
public:
  virtual void foo();
};

class Derived3 : public Base3 {
public:
  void foo() override {}
};

class Base4 {
public:
  void foo();
};

class Derived4 : public Base4 {
public:
  void foo() {}
};

class Mid : public Base3 {};

class Derived5 : public Mid {
public:
  void foo() override {}
};