// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/pylaeva_s_lab1_ClangAST%pluginext -plugin pylaeva_s_lab1_plugin -fsyntax-only %t/leak.cpp 2>&1 | FileCheck %t/leak.cpp
// RUN: %clang_cc1 -load %llvmshlibdir/pylaeva_s_lab1_ClangAST%pluginext -plugin pylaeva_s_lab1_plugin -fsyntax-only %t/no_leak.cpp 2>&1 | FileCheck %t/no_leak.cpp --allow-empty

//--- leak.cpp

extern "C" {
    void* malloc(unsigned long size);
    void* calloc(unsigned long count, unsigned long size);
    void* realloc(void* ptr, unsigned long size);
    void free(void* ptr);
    void* fopen(const char* filename, const char* mode);
    int fclose(void* stream);
}

// Проверка обнаружения простых утечек памяти внутри функций

// CHECK: warning: potential memory leak detected at line [[#@LINE+2]]
void test_calloc_simple_leak() {
    int *p_simple_calloc = (int*)calloc(10, sizeof(int));
}

// CHECK: warning: potential memory leak detected at line [[#@LINE+2]]
void test_new_simple_leak() {
    int* p_simple_new = new int;
}

// CHECK: warning: potential file handle leak detected at line [[#@LINE+2]]
void test_fopen_simple_leak() {
    void* f_simple = fopen("test.txt", "r");
}

// CHECK: warning: potential memory leak detected at line [[#@LINE+2]]
void test_malloc_simple_leak() {
    int* p_simple_malloc = (int*)malloc(sizeof(int));
}

// Проверка обнаружения простых утечек памяти на глобальном уровне

// CHECK: warning: potential memory leak detected at line [[#@LINE+1]]
int* leak_malloc = (int*)malloc(100);

// CHECK: warning: potential memory leak detected at line [[#@LINE+1]]
int* leak_сalloc = (int*)calloc(10, sizeof(int));

// CHECK: warning: potential memory leak detected at line [[#@LINE+1]]
int* leak_realloc = (int*)realloc(nullptr, 200);

// CHECK: warning: potential file handle leak detected at line [[#@LINE+1]]
void* file = fopen("test.txt", "r");

// CHECK: warning: potential memory leak detected at line [[#@LINE+1]]
int* leak_new1 = new int(42);

// CHECK: warning: potential memory leak detected at line [[#@LINE+1]]
int* leak_new2 = new int[100];

// Тесты на перезапись указателей

// CHECK: warning: potential memory leak detected at line [[#@LINE+3]]
void overwrite_test() {
    int* p = (int*)malloc(10);
    p = (int*)malloc(20); 
    free(p); 
}

// CHECK: warning: potential memory leak detected at line [[#@LINE+3]]
void overwrite_correct() {
    int* p1 = (int*)malloc(10);
    int* p2 = (int*)malloc(20);
    p1 = p2; 
    free(p1);
    
}

// Выделение памяти в сложном выражении
// CHECK: warning: potential memory leak detected at line [[#@LINE+2]]
void complex_expression_leak() {
    int* p = (int*)malloc(sizeof(int) * (10 + 20));
}

// Множественное выделение в одной строке
// CHECK: warning: potential memory leak detected at line [[#@LINE+2]]
void multiple_allocation_same_line() {
    int* a = (int*)malloc(10), *b = (int*)malloc(20);
    free(a); 
}

// Утечка памяти в if
// CHECK: warning: potential memory leak detected at line [[#@LINE+3]]
void if_leak(int x) {
    if (x > 0) {
        int* p = (int*)malloc(10);
    }    
}

// Выделение памяти в цикле, утечка в каждой итерации
void loop_leak() {
    for (int i = 0; i < 5; i++) {
        // CHECK: warning: potential memory leak detected at line [[#@LINE+1]]
        int* p = (int*)malloc(10);
    }
}



//--- no_leak.cpp

extern "C" {
    void* malloc(unsigned long size);
    void* calloc(unsigned long count, unsigned long size);
    void* realloc(void* ptr, unsigned long size);
    void free(void* ptr);
    void* fopen(const char* filename, const char* mode);
    int fclose(void* stream);
}

// CHECK-NOT: warning: potential memory leak
// CHECK-NOT: warning: potential file handle leak

void correct_malloc() {
    int* p = (int*)malloc(100);
    free(p);
}

void correct_calloc() {
    int* p = (int*)calloc(10, sizeof(int));
    free(p);
}

void correct_fopen() {
    void* f = fopen("test.txt", "r");
    fclose(f);
}

void correct_new1() {
    int* p = new int(64);
    delete p;
}

void correct_new2() {
    int* p = new int[10];
    delete[] p;
}

void correct_multiple() {
    int* a = (int*)malloc(10);
    int* b = new int(5);
    void* f = fopen("test.txt", "r"); 
    free(a);
    delete b;
    fclose(f);
}

void correct_if(int x) {
    int* p = (int*)malloc(10);
    if (x > 0) {
        free(p);
    } else {
        free(p);
    }
}

void loop_correct() {
    for (int i = 0; i < 5; i++) {
        int* p = (int*)malloc(10);
        free(p);
    }
}