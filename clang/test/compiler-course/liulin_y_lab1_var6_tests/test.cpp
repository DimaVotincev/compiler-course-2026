// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/liulin_y_lab1_var6_ClangAST%pluginext -plugin override_warning -Wno-inconsistent-missing-override -fsyntax-only -verify %t/with_warnings.cpp
// RUN: %clang_cc1 -load %llvmshlibdir/liulin_y_lab1_var6_ClangAST%pluginext -plugin override_warning -Wno-inconsistent-missing-override -fsyntax-only -verify %t/without_warnings.cpp

//--- with_warnings.cpp
// basic.cpp
class Base {
public:
  virtual void foo();
  virtual void bar() {}
};

class Derived : public Base {
public:
  void foo(); // expected-warning {{overriding virtual function without 'override' specifier}}
  void bar() override {} // ok
};

// pure-virtual.cpp
class Base2 {
public:
  virtual void foo() = 0;
};

class Derived2 : public Base2 {
public:
  void foo(); // expected-warning {{overriding virtual function without 'override' specifier}}
};

// multiple-inheritance.cpp
struct A {
  virtual void f();
};
struct B {
  virtual void f();
};
struct C : A, B {
  void f(); // expected-warning {{overriding virtual function without 'override' specifier}}
};

// templated.cpp
template <typename T>
struct Base3 {
  virtual void f(T);
};

struct Derived3 : Base3<int> {
  void f(int); // expected-warning {{overriding virtual function without 'override' specifier}}
};

// final.cpp
struct Base4 {
  virtual void f();
};
struct Derived4 final : Base4 {
  void f(); // expected-warning {{overriding virtual function without 'override' specifier}}
};

//--- without_warnings.cpp
// expected-no-diagnostics

// no-virtual.cpp
class Base5 {
public:
  void foo();
};

class Derived5 : public Base5 {
public:
  void foo(); 
};

// with-override.cpp
struct Base6 {
  virtual void f();
};
struct Derived6 : Base6 {
  void f() override; // ok
};

// multiple-overridden.cpp
struct A2 {
  virtual void f();
};
struct B2 {
  virtual void f();
};
struct C2 : A2, B2 {
  void f() override; // ok
};

// no-warning-for-non-override.cpp
struct Base7 {
  virtual void f();
};
struct Derived7 : Base7 {
  virtual void g(); 
};