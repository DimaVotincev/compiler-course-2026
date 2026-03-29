// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/dorofeev_i_cast_replace_ClangAST%pluginext -plugin cast_replace_plugin -fsyntax-only %t/test_1.cpp 2>&1 | FileCheck %s

// CHECK-LABEL: void test_primitive_casts() {
// CHECK-NEXT:      int a = 5;
// CHECK-NEXT:      double b = static_cast<double>(a);
// CHECK-NEXT:      int *p = reinterpret_cast<int *>(a);
// CHECK-NEXT:      const int c = 10;
// CHECK-NEXT:      int *q = const_cast<int *>(&c);
// CHECK-NEXT:  }

// CHECK-LABEL: void test_custom_types() {
// CHECK-NEXT:      Point pt = {10, 20};
// CHECK-NEXT:      DataBlock *data = reinterpret_cast<DataBlock *>(&pt);
// CHECK-NEXT:      Derived derived_obj;
// CHECK-NEXT:      Base *base_ptr = static_cast<Base *>(&derived_obj);
// CHECK-NEXT:      Base *b_ptr = new Derived();
// CHECK-NEXT:      Derived *d_ptr = static_cast<Derived *>(b_ptr);
// CHECK-NEXT:      const Point const_pt = {0, 0};
// CHECK-NEXT:      Point *mut_pt = const_cast<Point *>(&const_pt);
// CHECK-NEXT:  }

//--- test_1.cpp

struct Point {
    int x;
    int y;
};

struct DataBlock {
    float values[4];
};

class Base {
public:
    virtual ~Base() {}
};

class Derived : public Base {
public:
    int id;
};

void test_primitive_casts() {
    int a = 5;
    double b = (double)a;
    int *p = (int *)a;
    const int c = 10;
    int *q = (int *)&c;
}

void test_custom_types() {
    Point pt = {10, 20};
    DataBlock *data = (DataBlock *)&pt;
    Derived derived_obj;
    Base *base_ptr = (Base *)&derived_obj;
    Base *b_ptr = new Derived();
    Derived *d_ptr = (Derived *)b_ptr;
    const Point const_pt = {0, 0};
    Point *mut_pt = (Point *)&const_pt;
}