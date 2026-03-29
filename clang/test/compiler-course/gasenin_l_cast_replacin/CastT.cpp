// RUN: split-file %s %t

// RUN: %clang_cc1 -load %llvmshlibdir/gasenin_l_cast_replacin_ClangAST%pluginext -plugin cast_replace_plugin -fsyntax-only %t/test_arithmetic.cpp 2>&1 | FileCheck %s --check-prefix=ARITH
// ARITH-LABEL: test_arithmetic()
// ARITH: int i = static_cast<int>(d);
// ARITH: double d2 = static_cast<double>(i);
// ARITH: long l = static_cast<long>(i);
// ARITH: float f = static_cast<float>(d);
//--- test_arithmetic.cpp
void test_arithmetic() {
    double d = 3.14;
    int i = (int)d;
    double d2 = (double)i;
    long l = (long)i;
    float f = (float)d;
}

// RUN: %clang_cc1 -load %llvmshlibdir/gasenin_l_cast_replacin_ClangAST%pluginext -plugin cast_replace_plugin -fsyntax-only %t/test_void_pointer.cpp 2>&1 | FileCheck %s --check-prefix=VOID
// VOID-LABEL: test_void_pointer()
// VOID: void *vp = static_cast<void *>(p);
// VOID: int *p2 = static_cast<int *>(vp);
//--- test_void_pointer.cpp
void test_void_pointer() {
    int x = 0;
    int *p = &x;
    void *vp = (void*)p;
    int *p2 = (int*)vp;
}

// RUN: %clang_cc1 -load %llvmshlibdir/gasenin_l_cast_replacin_ClangAST%pluginext -plugin cast_replace_plugin -fsyntax-only %t/test_const_cast.cpp 2>&1 | FileCheck %s --check-prefix=CONST
// CONST-LABEL: test_const_cast()
// CONST: int *p = const_cast<int *>(cp);
// CONST: char *mp = const_cast<char *>(ccp);
// CONST: int j = const_cast<int>(ci);
//--- test_const_cast.cpp
void test_const_cast() {
    int x = 0;
    const int *cp = &x;
    int *p = (int*)cp;

    const char *ccp = "hello";
    char *mp = (char*)ccp;

    const int ci = 42;
    int j = (int)ci;
}

// RUN: %clang_cc1 -load %llvmshlibdir/gasenin_l_cast_replacin_ClangAST%pluginext -plugin cast_replace_plugin -fsyntax-only %t/test_reinterpret.cpp 2>&1 | FileCheck %s --check-prefix=REINT
// REINT-LABEL: test_reinterpret()
// REINT: char *cp = reinterpret_cast<char *>(ip);
// REINT: long addr = reinterpret_cast<long>(ip);
// REINT: int *ip2 = reinterpret_cast<int *>(addr);
// REINT: char &cr = reinterpret_cast<char &>(x);
//--- test_reinterpret.cpp
void test_reinterpret() {
    int x = 42;
    int *ip = &x;
    char *cp = (char*)ip;
    long addr = (long)ip;
    int *ip2 = (int*)addr;
    char &cr = (char&)x;
}

// RUN: %clang_cc1 -load %llvmshlibdir/gasenin_l_cast_replacin_ClangAST%pluginext -plugin cast_replace_plugin -fsyntax-only %t/test_hierarchy.cpp 2>&1 | FileCheck %s --check-prefix=HIER
// HIER-LABEL: test_hierarchy()
// HIER: Base *base_ptr = static_cast<Base *>(&derived_obj);
// HIER: Derived *d_ptr = dynamic_cast<Derived *>(b_ptr);
// HIER: NpDerived *np_d = static_cast<NpDerived *>(np_base);
// HIER: VBase *vb = static_cast<VBase *>(&vd);
// HIER: DataBlock *data = reinterpret_cast<DataBlock *>(&pt);
// HIER: Point *mut_pt = const_cast<Point *>(&cpt);
//--- test_hierarchy.cpp
struct Point     { int x, y; };
struct DataBlock { float v[4]; };

class Base     { public: virtual ~Base() = default; };
class Derived  : public Base { public: int id; };

class NpBase    { public: int val; };
class NpDerived : public NpBase { public: int extra; };

class VBase    { public: int data; };
class VDerived : virtual public VBase {};

void test_hierarchy() {
    Derived derived_obj;
    Base *base_ptr = (Base *)&derived_obj;

    Base *b_ptr = new Derived();
    Derived *d_ptr = (Derived *)b_ptr;

    NpBase *np_base = new NpDerived();
    NpDerived *np_d = (NpDerived *)np_base;

    VDerived vd;
    VBase *vb = (VBase *)&vd;

    Point pt = {1, 2};
    DataBlock *data = (DataBlock *)&pt;

    const Point cpt = {0, 0};
    Point *mut_pt = (Point *)&cpt;
}

// RUN: %clang_cc1 -load %llvmshlibdir/gasenin_l_cast_replacin_ClangAST%pluginext -plugin cast_replace_plugin -fsyntax-only %t/test_to_bool.cpp 2>&1 | FileCheck %s --check-prefix=BOOL
// BOOL-LABEL: test_to_bool()
// BOOL: bool bi = static_cast<bool>(i);
// BOOL: bool bd = static_cast<bool>(d);
// BOOL: bool bp = static_cast<bool>(p);
//--- test_to_bool.cpp
void test_to_bool() {
    int    i = 1;
    double d = 3.14;
    int   *p = &i;
    bool bi = (bool)i;
    bool bd = (bool)d;
    bool bp = (bool)p;
}

// RUN: %clang_cc1 -load %llvmshlibdir/gasenin_l_cast_replacin_ClangAST%pluginext -plugin cast_replace_plugin -fsyntax-only %t/test_to_void.cpp 2>&1 | FileCheck %s --check-prefix=TOVOID
// TOVOID-LABEL: test_to_void()
// TOVOID: static_cast<void>(x);
//--- test_to_void.cpp
void test_to_void() {
    int x = 0;
    (void)x;
}

// RUN: %clang_cc1 -load %llvmshlibdir/gasenin_l_cast_replacin_ClangAST%pluginext -plugin cast_replace_plugin -fsyntax-only %t/test_null_casts.cpp 2>&1 | FileCheck %s --check-prefix=NULL
// NULL-LABEL: test_null_casts()
// NULL: int *np = static_cast<int *>(0);
// NULL: int Holder::*mp = static_cast<int Holder::*>(0);
//--- test_null_casts.cpp
struct Holder { int value; };

void test_null_casts() {
    int *np = (int*)0;
    int Holder::*mp = (int Holder::*)0;
}

// RUN: %clang_cc1 -load %llvmshlibdir/gasenin_l_cast_replacin_ClangAST%pluginext -plugin cast_replace_plugin -fsyntax-only %t/test_member_pointer_reinterpret.cpp 2>&1 | FileCheck %s --check-prefix=MEMPTR
// MEMPTR-LABEL: test_member_pointer_reinterpret()
// MEMPTR: int MpDerived::*mdp = reinterpret_cast<int MpDerived::*>(mbp);
//--- test_member_pointer_reinterpret.cpp
struct MpBase    { int a; };
struct MpDerived { int b; };

void test_member_pointer_reinterpret() {
    int MpBase::*mbp = &MpBase::a;
    int MpDerived::*mdp = (int MpDerived::*)mbp;
}

// RUN: %clang_cc1 -load %llvmshlibdir/gasenin_l_cast_replacin_ClangAST%pluginext -plugin cast_replace_plugin -fsyntax-only %t/test_macro_skip.cpp 2>&1 | FileCheck %s --check-prefix=MACRO
// MACRO-LABEL: test_macro_skip()
// MACRO: int m = MACRO_CAST(int, d);
//--- test_macro_skip.cpp
#define MACRO_CAST(T, v) (T)(v)

void test_macro_skip() {
    double d = 2.71;
    int m = MACRO_CAST(int, d);
}
