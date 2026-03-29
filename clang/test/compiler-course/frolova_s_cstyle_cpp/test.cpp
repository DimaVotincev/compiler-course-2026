// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/CStyleCastReplacerPlugin_Frolova_Sofya_FIIT3_ClangAST%pluginext -add-plugin cstyle_cast_replacer %t/test_casts.cpp 2>&1 | FileCheck %s

// CHECK: int i = static_cast<int>(d);
// CHECK: int i2 = static_cast<int>(ci);
// CHECK: int* pi = const_cast<int *>(pci);
// CHECK: const int* pci2 = const_cast<const int *>(pi2);
// CHECK: int& ri = const_cast<int &>(rci);
// CHECK: const int& rci2 = const_cast<const int &>(ri2);
// CHECK: int i = static_cast<int>(vi);
// CHECK: int* pi = const_cast<int *>(pvi);
// CHECK: int* pi2 = const_cast<int *>(pcvi);
// CHECK: char* pc = reinterpret_cast<char *>(&i);
// CHECK: unsigned long addr = reinterpret_cast<unsigned long>(pc);
// CHECK: int* pi = reinterpret_cast<int *>(addr);
// CHECK: char& rc = reinterpret_cast<char &>(i);
// CHECK: int& ir = reinterpret_cast<int &>(rc);

//--- test_casts.cpp
void test_casts() {
    double d = 10.5;

    int i = (int)d;


    const int ci = 5;
    int i2 = (int)ci;
}

void test_const_casts() {
    const int* pci = nullptr;
    int* pi = (int*)pci;                 

    int* pi2 = nullptr;
    const int* pci2 = (const int*)pi2;   
    const int& rci = 5;
    int& ri = (int&)rci;                 

    int x = 42;
    int& ri2 = x;
    const int& rci2 = (const int&)ri2;    
}

void test_volatile_casts() {
    volatile int vi = 10;
    int i = (int)vi;                     

    volatile int* pvi = nullptr;
    int* pi = (int*)pvi;

    const volatile int* pcvi = nullptr;
    int* pi2 = (int*)pcvi;
}

void test_reinterpret_casts() {
    int i = 42;
    char* pc = (char*)&i;

    unsigned long addr = (unsigned long)pc;

    int* pi = (int*)addr;

    char& rc = (char&)i;

    int& ir = (int&)rc;
}