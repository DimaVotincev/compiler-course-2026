// RUN: %clang_cc1 -load %llvmshlibdir/shekhirev_v_cast_replace_ClangAST%pluginext -plugin shekhirev_v_cast_replace_plugin -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK-LABEL: void test_static_basic() {
// CHECK: float y = static_cast<float>(x);
void test_static_basic() {
  int x = 10;
  float y = (float)x;
}

// CHECK-LABEL: void test_static_expr() {
// CHECK: double d = static_cast<double>((a + b));
void test_static_expr() {
  int a = 5, b = 3;
  double d = (double)(a + b);
}

// CHECK-LABEL: void test_static_void_ptr() {
// CHECK: void *v = static_cast<void *>(ptr);
void test_static_void_ptr() {
  int *ptr = nullptr;
  void *v = (void *)ptr;
}

// CHECK-LABEL: void test_nested_casts() {
// CHECK: float f = static_cast<float>(static_cast<long>(x));
void test_nested_casts() {
  int x = 42;
  float f = (float)(long)x;
}

// CHECK-LABEL: void test_const_remove() {
// CHECK: int *p = const_cast<int *>(&x);
void test_const_remove() {
  const int x = 10;
  int *p = (int *)&x;
}

// CHECK-LABEL: void test_const_add() {
// CHECK: const int *cp = const_cast<const int *>(&x);
void test_const_add() {
  int x = 10;
  const int *cp = (const int *)&x;
}

// CHECK-LABEL: void test_reinterpret_ptr_to_int() {
// CHECK: unsigned long addr = reinterpret_cast<unsigned long>(&x);
void test_reinterpret_ptr_to_int() {
  int x = 42;
  unsigned long addr = (unsigned long)&x;
}

// CHECK-LABEL: void test_reinterpret_int_to_ptr() {
// CHECK: int *p = reinterpret_cast<int *>(addr);
void test_reinterpret_int_to_ptr() {
  unsigned long addr = 0x1234;
  int *p = (int *)addr;
}

// CHECK-LABEL: void test_reinterpret_unrelated_ptrs() {
// CHECK: float *f = reinterpret_cast<float *>(i);
void test_reinterpret_unrelated_ptrs() {
  int *i = nullptr;
  float *f = (float *)i;
}

class Base {
public:
  virtual ~Base() {}
};

class Derived : public Base {
public:
  int id;
};

// CHECK-LABEL: void test_dynamic_downcast() {
// CHECK: Derived *d = dynamic_cast<Derived *>(b);
void test_dynamic_downcast() {
  Base *b = new Derived();
  Derived *d = (Derived *)b;
}

// CHECK-LABEL: void test_dynamic_upcast() {
// CHECK: Base *b = static_cast<Base *>(d);
void test_dynamic_upcast() {
  Derived *d = new Derived();
  Base *b = (Base *)d;
}