// RUN: %clang_cc1 -load %llvmshlibdir/kazennova_a_lab1_ClangAST%pluginext -plugin kazennova_a_lab1_plugin -fsyntax-only %s 2>&1 | FileCheck %s

// для static_cast

// CHECK-LABEL: test_simple
void test_simple() {
  int x = 10;
  float y = (float)x;
  // CHECK: float y = static_cast<float>(x);
}

// CHECK-LABEL: test_expression
void test_expression() {
  int a = 5, b = 3;
  double d = (double)(a + b);
  // CHECK: double d = static_cast<double>(a + b);
}

// CHECK-LABEL: test_pointer
void test_pointer() {
  int *ptr = 0;
  void *v = (void *)ptr;
  // CHECK: void *v = static_cast<void *>(ptr);
}

// CHECK-LABEL: test_multiple
void test_multiple() {
  int x = 10, y = 20;
  float f = (float)x + (float)y;
  // CHECK: float f = static_cast<float>(x) + static_cast<float>(y);
}

// CHECK-LABEL: test_complex
void test_complex() {
  int x = 1, y = 2;
  float f = (float)(x + y) * (float)(x - y);
  // CHECK: float f = static_cast<float>(x + y) * static_cast<float>(x - y);
}

// для const_cast

// CHECK-LABEL: test_const_remove
void test_const_remove() {
  const int x = 10;
  int *p = (int *)&x;
  // CHECK: int *p = const_cast<int *>(&x);
}

// CHECK-LABEL: test_const_add
void test_const_add() {
  int x = 10;
  const int *cp = (const int *)&x;
  // CHECK: const int *cp = const_cast<const int *>(&x);
}

// для reinterpret_cast

// CHECK-LABEL: test_reinterpret_ptr_to_int
void test_reinterpret_ptr_to_int() {
  int x = 42;
  unsigned long addr = (unsigned long)&x;
  // CHECK: unsigned long addr = reinterpret_cast<unsigned long>(&x);
}

// CHECK-LABEL: test_reinterpret_int_to_ptr
void test_reinterpret_int_to_ptr() {
  unsigned long addr = 0x1234;
  int *p = (int *)addr;
  // CHECK: int *p = reinterpret_cast<int *>(addr);
}

// для dynamic_cast

class Base {
public:
  virtual ~Base() {}
};

class Derived : public Base {
public:
  int id;
};

// CHECK-LABEL: test_dynamic_downcast
void test_dynamic_downcast() {
  Base *b = new Derived();
  Derived *d = (Derived *)b;
  // CHECK: Derived *d = dynamic_cast<Derived *>(b);
}

// CHECK-LABEL: test_dynamic_upcast
void test_dynamic_upcast() {
  Derived *d = new Derived();
  Base *b = (Base *)d;
  // CHECK: Base *b = dynamic_cast<Base *>(d);
}