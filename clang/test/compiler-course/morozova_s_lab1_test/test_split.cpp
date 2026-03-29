// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/VarStatPlugin.so -plugin var-stat -fsyntax-only %t/basic_test.cpp 2>&1 | FileCheck %t/basic_test.cpp --check-prefix=BASIC
// RUN: %clang_cc1 -load %llvmshlibdir/VarStatPlugin.so -plugin var-stat -fsyntax-only %t/class_test.cpp 2>&1 | FileCheck %t/class_test.cpp --check-prefix=CLASS
// RUN: %clang_cc1 -load %llvmshlibdir/VarStatPlugin.so -plugin var-stat -fsyntax-only %t/multiple_test.cpp 2>&1 | FileCheck %t/multiple_test.cpp --check-prefix=MULTIPLE
// RUN: %clang_cc1 -load %llvmshlibdir/VarStatPlugin.so -plugin var-stat -fsyntax-only %t/namespace_test.cpp 2>&1 | FileCheck %t/namespace_test.cpp --check-prefix=NAMESPACE
// RUN: %clang_cc1 -load %llvmshlibdir/VarStatPlugin.so -plugin var-stat -fsyntax-only %t/test1.cpp 2>&1 | FileCheck %t/test1.cpp --check-prefix=MAIN

//--- basic_test.cpp
// BASIC: ========== СТАТИСТИКА ПЕРЕМЕННЫХ ==========
// BASIC: Глобальных переменных: 1
// BASIC: Локальных переменных: 1
// BASIC: Статических локальных: 1
// BASIC: Параметров функций: 1
// BASIC: ===========================================

int global = 10;

void func(int param) {
    static int staticLocal = 5;
    int local = 42;
}

//--- class_test.cpp
// CLASS: ========== СТАТИСТИКА ПЕРЕМЕННЫХ ==========
// CLASS: Глобальных переменных: 1
// CLASS: Локальных переменных: 0
// CLASS: Статических локальных: 0
// CLASS: Параметров функций: 2
// CLASS: ===========================================

class MyClass {
public:
    MyClass(int val) : value(val) {}
    int value;
    static int count;
};

int MyClass::count = 0;

struct MyStruct {
    MyStruct(int num = 0) : data(num) {}
    int data;
};

//--- multiple_test.cpp
// MULTIPLE: ========== СТАТИСТИКА ПЕРЕМЕННЫХ ==========
// MULTIPLE: Глобальных переменных: 4
// MULTIPLE: Локальных переменных: 3
// MULTIPLE: Статических локальных: 2
// MULTIPLE: Параметров функций: 2
// MULTIPLE: ===========================================

int a, b;
static int c;
const int d = 5;

void foo(int x, int y) {
    int l1, l2, l3;
    static int s1, s2;
}

//--- namespace_test.cpp
// NAMESPACE: ========== СТАТИСТИКА ПЕРЕМЕННЫХ ==========
// NAMESPACE: Глобальных переменных: 4
// NAMESPACE: Локальных переменных: 0
// NAMESPACE: Статических локальных: 0
// NAMESPACE: Параметров функций: 0
// NAMESPACE: ===========================================

namespace N {
    int x;
    static int y;
}

namespace {
    int z;
    static int w;
}

//--- test1.cpp
// MAIN: ========== СТАТИСТИКА ПЕРЕМЕННЫХ ==========
// MAIN: Глобальных переменных: 23
// MAIN: Локальных переменных: 6
// MAIN: Статических локальных: 5
// MAIN: Параметров функций: 9
// MAIN: ===========================================

int g1;
int g2 = 5;
int g3, g4, g5;
static int sg1;
extern int eg1;
const int cg1 = 10;
constexpr int ceg1 = 20;

namespace N {
    int n_g1;
    static int n_sg1;
}

namespace {
    int anon_g1;
    static int anon_sg1;
}

extern int duplicate;
int duplicate;

void func1(int p1) {}
void func2(int p1, int p2) {}
void func3(int p1, int p2, int p3, int p4) {}

void test_locals() {
    int l1;
    int l2 = 42;
    int l3, l4, l5;
    static int sl1;
    static int sl2 = 100;
}

void test_more_static() {
    static int sl3;
    static int sl4;
}

template<typename T>
T template_func(T a, T b) {
    static int tls;
    int tl = 42;
    return a + b;
}

class MyClass {
public:
    MyClass(int v) : value(v) {}
    int value;
    static int class_static;
};

int MyClass::class_static = 0;

struct Struct {
    Struct(int num = 0) : a(num), k(0.0) {}
    int a;
    double k;
};

extern int repeat;
int repeat;

const int ci = 5;
constexpr int cei = 10;
volatile int vi = 0;

int multi1, multi2, multi3;

Struct s1;
Struct s2(42);

void empty() {}
