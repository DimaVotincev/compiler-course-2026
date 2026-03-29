// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/krykov_e_c_cast_ClangAST%pluginext -plugin replace_c_cast %t/CastTest.cpp 2>&1 | FileCheck %s

// CHECK-LABEL: void primitive_tests() {
// CHECK-NEXT:   int a = 5;
// CHECK-NEXT:   double b = static_cast<double>(a);
// CHECK-NEXT:   const int c = 10;
// CHECK-NEXT:   int *q = const_cast<int *>(&c);
// CHECK-NEXT:   int *p = reinterpret_cast<int *>(a);
// CHECK-NEXT: }

// CHECK-LABEL: void user_type_tests() {
// CHECK-NEXT:   Vec2 v{1,2};
// CHECK-NEXT:   RawBlock *block = reinterpret_cast<RawBlock *>(&v);
// CHECK-NEXT:   Child child;
// CHECK-NEXT:   Parent *p = static_cast<Parent *>(&child);
// CHECK-NEXT:   Parent *base = new Child();
// CHECK-NEXT:   Child *c = static_cast<Child *>(base);
// CHECK-NEXT:   const Vec2 const_v{0,0};
// CHECK-NEXT:   Vec2 *mutable_v = const_cast<Vec2 *>(&const_v);
// CHECK-NEXT: }

//--- CastTest.cpp
void primitive_tests() {
  int a = 5;
  double b = (double)a;
  const int c = 10;
  int *q = (int *)&c;
  int *p = (int *)a;  
}

struct Vec2 {
  int x;
  int y;
};

struct RawBlock {
  float data[2];
};

class Parent {
public:
  virtual ~Parent() {}
};

class Child : public Parent {
public:
  int value;
};

void user_type_tests() {
  Vec2 v{1,2};
  RawBlock *block = (RawBlock *)&v;
  Child child;
  Parent *p = (Parent *)&child;
  Parent *base = new Child();
  Child *c = (Child *)base;
  const Vec2 const_v{0,0};
  Vec2 *mutable_v = (Vec2 *)&const_v;
}