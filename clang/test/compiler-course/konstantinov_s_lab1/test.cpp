// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/konstantinov_s_lab1_ClangAST%pluginext -plugin cast_rewrite_plugin -fsyntax-only %t/source.cpp 2>&1 | FileCheck %s

// CHECK-LABEL: int convertFloatToInt(float x)
// CHECK-NEXT:     return static_cast<int>(x);

// CHECK-LABEL: int convertNegativeDouble(double x)
// CHECK-NEXT:     return static_cast<int>(-x);

// CHECK-LABEL: bool compareAfterCast(float x)
// CHECK-NEXT:     return static_cast<int>(x) > 0;

// CHECK-LABEL: long long convertFloatToLongLong(float x)
// CHECK-NEXT:     return static_cast<long long>(x);

// CHECK-LABEL: double castSum(long long x, long long y)
// CHECK-NEXT:     return static_cast<double>((x + y));

// CHECK-LABEL: int *floatPtrToIntPtr(float *p)
// CHECK-NEXT:     return reinterpret_cast<int *>(p);

// CHECK-LABEL: long ptrToLong(int *p)
// CHECK-NEXT:     return reinterpret_cast<long>(p);

// CHECK-LABEL: int *longToPtr(long value)
// CHECK-NEXT:     return reinterpret_cast<int *>(value);

// CHECK-LABEL: void *toVoidPtr(int *p)
// CHECK-NEXT:     return static_cast<void *>(p);

// CHECK-LABEL: int *fromVoidPtr(void *p)
// CHECK-NEXT:     return static_cast<int *>(p);

// CHECK-LABEL: char *floatPtrToCharPtr(float *p)
// CHECK-NEXT:     return (reinterpret_cast<char *>((p)));

// CHECK-LABEL: int *removeConst(const int *p)
// CHECK-NEXT:     return const_cast<int *>(p);

// CHECK-LABEL: const int *addConst(int *p)
// CHECK-NEXT:     return const_cast<const int *>(p);

// CHECK-LABEL: int enumToNumber(Color c)
// CHECK-NEXT:     return static_cast<int>(c);

// CHECK-LABEL: Color numberToEnum(int value)
// CHECK-NEXT:     return static_cast<Color>(value);

// CHECK-LABEL: Base *derivedToBase(Derived *obj)
// CHECK-NEXT:     return static_cast<Base *>(obj);

// CHECK-LABEL: Derived *baseToDerived(Base *obj)
// CHECK-NEXT:     return static_cast<Derived *>(obj);

// CHECK-LABEL: struct Container
// CHECK-NEXT:     int field = static_cast<int>(2.75f);
// CHECK-NEXT:     int method(float x)
// CHECK-NEXT:         return static_cast<int>(x);

// CHECK-LABEL: void assignCast(float x)
// CHECK-NEXT:     int local = static_cast<int>(x);

// CHECK-LABEL: const float GlobalValue = static_cast<float>(64);

//--- source.cpp

int convertFloatToInt(float x) {
    return (int)x;
}

int convertNegativeDouble(double x) {
    return (int)-x;
}

bool compareAfterCast(float x) {
    return (int)x > 0;
}

long long convertFloatToLongLong(float x) {
    return (long long)x;
}

double castSum(long long x, long long y) {
    return (double)(x + y);
}

int *floatPtrToIntPtr(float *p) {
    return (int *)p;
}

long ptrToLong(int *p) {
    return (long)p;
}

int *longToPtr(long value) {
    return (int *)value;
}

void *toVoidPtr(int *p) {
    return (void *)p;
}

int *fromVoidPtr(void *p) {
    return (int *)p;
}

char *floatPtrToCharPtr(float *p) {
    return ((char *)(p));
}

int *removeConst(const int *p) {
    return (int *)p;
}

const int *addConst(int *p) {
    return (const int *)p;
}

enum Color { Red, Green, Blue };

int enumToNumber(Color c) {
    return (int)c;
}

Color numberToEnum(int value) {
    return (Color)value;
}

struct Base {
  virtual ~Base() {}
};

struct Derived : Base {
  int id;
};

Base *derivedToBase(Derived *obj) {
    return (Base *)obj;
}

Derived *baseToDerived(Base *obj) {
    return (Derived *)obj;
}

struct Container {
    int field = (int)2.75f;
    int method(float x) {
        return (int)x;
    }
};

void assignCast(float x) {
    int local = (int)x;
}

const float GlobalValue = (float)64;
