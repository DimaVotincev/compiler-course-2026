// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/sannikov_i_c_style_cast_ClangAST%pluginext -plugin cast_rewriter -fsyntax-only %t/test_arithmetic.cpp 2>&1 | FileCheck %s --check-prefix=ARITH
// ARITH-LABEL: test_arithmetic(
// ARITH: int i = static_cast<int>(d);
// ARITH: long l = static_cast<long>(i);
// ARITH: float f = static_cast<float>(d);

//--- test_arithmetic.cpp
void test_arithmetic() {
  double d = 2.47;
  int i = (int)d;
  long l = (long)i;
  float f = (float)d;
  (void)i;
  (void)l;
  (void)f;
}

// RUN: %clang_cc1 -load %llvmshlibdir/sannikov_i_c_style_cast_ClangAST%pluginext -plugin cast_rewriter -fsyntax-only %t/test_const.cpp 2>&1 | FileCheck %s --check-prefix=CONST
// CONST-LABEL: test_const_ptr(
// CONST: int *p = const_cast<int*>(src);
// CONST: return *p;
// CONST-LABEL: test_const_ref(
// CONST: int &r = const_cast<int&>(src);
// CONST: static_cast<void>(r);
//--- test_const.cpp
int test_const_ptr() {
  int value = 7;
  const int *src = &value;
  int *p = (int*)src;
  return *p;
}

void test_const_ref() {
  int value = 11;
  const int &src = value;
  int &r = (int&)src;
  (void)r;
}

// RUN: %clang_cc1 -load %llvmshlibdir/sannikov_i_c_style_cast_ClangAST%pluginext -plugin cast_rewriter -fsyntax-only %t/test_reinterpret.cpp 2>&1 | FileCheck %s --check-prefix=REINT
// REINT-LABEL: test_reinterpret(
// REINT: long bits = reinterpret_cast<long>(ptr);
// REINT: int *p = reinterpret_cast<int*>(raw);
// REINT: double *q = reinterpret_cast<double*>(p);
//--- test_reinterpret.cpp
void test_reinterpret() {
  int value = 42;
  int *ptr = &value;
  long raw = 1024;
  long bits = (long)ptr;
  int *p = (int*)raw;
  double *q = (double*)p;
  (void)bits;
  (void)q;
}
// RUN: %clang_cc1 -load %llvmshlibdir/sannikov_i_c_style_cast_ClangAST%pluginext -plugin cast_rewriter -fsyntax-only %t/test_dynamic.cpp 2>&1 | FileCheck %s --check-prefix=DYN
// DYN-LABEL: test_dynamic(
// DYN: Derived *d = dynamic_cast<Derived*>(b);
// DYN: return d;
//--- test_dynamic.cpp
struct Base {
  virtual ~Base() = default;
};

struct Derived : Base {};

Derived *test_dynamic() {
  Base *b = new Derived();
  Derived *d = (Derived*)b;
  return d;
}
// RUN: %clang_cc1 -load %llvmshlibdir/sannikov_i_c_style_cast_ClangAST%pluginext -plugin cast_rewriter -fsyntax-only %t/test_mixed.cpp 2>&1 | FileCheck %s --check-prefix=MIX
// MIX-LABEL: test_mixed(
// MIX: int x = static_cast<int>(d);
// MIX: int *p = const_cast<int*>(src);
// MIX: long raw = reinterpret_cast<long>(p);
// MIX: return x + static_cast<int>(raw);
//--- test_mixed.cpp
int test_mixed() {
  double d = 8.25;
  int value = 5;
  const int *src = &value;
  int x = (int)d;
  int *p = (int*)src;
  long raw = (long)p;
  return x + (int)raw;
}

// RUN: %clang_cc1 -load %llvmshlibdir/sannikov_i_c_style_cast_ClangAST%pluginext -plugin cast_rewriter -fsyntax-only %t/test_void_ptr.cpp 2>&1 | FileCheck %s --check-prefix=VOID
// VOID-LABEL: test_void_ptr(
// VOID: int *p = static_cast<int*>(vptr);
// VOID: void *q = static_cast<void*>(ptr);
// VOID: char *c = static_cast<char*>(q);
// VOID: return c;
//--- test_void_ptr.cpp
char *test_void_ptr() {
  int value = 1;
  int *ptr = &value;
  void *vptr = ptr;
  int *p = (int*)vptr;
  void *q = (void*)ptr;
  char *c = (char*)q;
  (void)p;
  return c;
}