// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/lifanov_k_analyzer_ClangAST%pluginext -plugin lifanov_k_resource-checker -fsyntax-only -verify %t/with_warnings.cpp
// RUN: %clang_cc1 -load %llvmshlibdir/lifanov_k_analyzer_ClangAST%pluginext -plugin lifanov_k_resource-checker -fsyntax-only -verify %t/without_warnings.cpp

//--- with_warnings.cpp
extern "C" {
    void* malloc(unsigned long size);
    void free(void* ptr);
    void* fopen(const char* filename, const char* mode);
    int fclose(void* stream);
}

int* global_leak = (int*)malloc(100);

int* test_malloc_return(int n) {
    int* p = (int*)malloc(n);
    return p; // expected-warning {{ресурс для переменной 'p' может быть не освобождён при выходе}} expected-warning {{ресурс для переменной 'global_leak' может быть не освобождён при выходе}}
}

int* test_new_return(int n) {
    int* arr = new int[n];
    return arr; // expected-warning {{ресурс для переменной 'arr' может быть не освобождён при выходе}}
}

void* test_file_return(const char* name) {
    void* f = fopen(name, "r");
    return f; // expected-warning {{ресурс для переменной 'f' может быть не освобождён при выходе}}
}

void test_branch_leak(int n, int value) {
    int* data = new int[n];

    if (value < 0) {
        return; // expected-warning {{ресурс для переменной 'data' может быть не освобождён при выходе}}
    }

    delete[] data;
}

void test_nested_scope(int n) {
    {
        int* p = (int*)malloc(n);
    }

    return; // expected-warning {{ресурс для переменной 'p' может быть не освобождён при выходе}}
}

void test_no_return_leak(int n) {
    int* p = (int*)malloc(n); // expected-warning {{выделенный ресурс для 'p' не освобождён}}
    
}

void test_new_array_no_delete(int n) {
    int* arr = new int[n]; // expected-warning {{выделенный ресурс для 'arr' не освобождён}}
    
}

//--- without_warnings.cpp
// expected-no-diagnostics

extern "C" {
    void* malloc(unsigned long size);
    void free(void* ptr);
    void* fopen(const char* filename, const char* mode);
    int fclose(void* stream);
}

void test_malloc_ok(int n) {
    int* p = (int*)malloc(n);
    free(p);
}

void test_new_ok(int n) {
    int* p = new int[n];
    delete[] p;
}

void test_file_ok(const char* name) {
    void* f = fopen(name, "r");
    fclose(f);
}

void test_branching_ok(int n, int v) {
    int* p = new int[n];

    if (v > n) {
        delete[] p;
        return;
    }

    delete[] p;
}

void test_nested_ok(int n) {
    {
        int* p = (int*)malloc(n);
        free(p);
    }
}