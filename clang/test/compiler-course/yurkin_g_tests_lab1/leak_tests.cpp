// leak_tests.cpp
// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/yurkin_g_lab1_ClangAST%pluginext -plugin leak_checker -fsyntax-only -verify %t/with_warnings.cpp
// RUN: %clang_cc1 -load %llvmshlibdir/yurkin_g_lab1_ClangAST%pluginext -plugin leak_checker -fsyntax-only -verify %t/without_warnings.cpp

//--- with_warnings.cpp
extern "C" {
    void* malloc(unsigned long size);
    void* fopen(const char* filename, const char* mode);
}

int* g_leak = (int*)malloc(100); // expected-warning {{Память или ресурс для переменной 'g_leak' не освобождены!}}

int* return_malloc_leak(int n) {
    int* q = (int*)malloc(n);
    return q; // expected-warning {{Ресурс для переменной 'q' может быть не освобожден (не гарантированное освобождение при return)!}}
}

int* return_new_leak(int n) {
    int* r = new int[n];
    return r; // expected-warning {{Ресурс для переменной 'r' может быть не освобожден (не гарантированное освобождение при return)!}}
}

void* fopen_return_leak(const char* path) {
    void* fh = fopen(path, "r");
    return fh; // expected-warning {{Ресурс для переменной 'fh' может быть не освобожден (не гарантированное освобождение при return)!}}
}

void early_exit_leak(int n, bool cond) {
    int* arr = new int[n];
    if (cond) {
        return; // expected-warning {{Ресурс для переменной 'arr' может быть не освобожден (не гарантированное освобождение при return)!}}
    }
    delete[] arr;
}

void nested_scope_leak(int n) {
    {
        {
            int* nested_ptr = (int*)malloc(n); // expected-warning {{Память или ресурс для переменной 'nested_ptr' не освобождены!}}
        }
    }
}

void missing_free_simple(int n) {
    int* local_p = (int*)malloc(n); // expected-warning {{Память или ресурс для переменной 'local_p' не освобождены!}}
}

void missing_delete_simple(int n) {
    int* local_arr = new int[n]; // expected-warning {{Память или ресурс для переменной 'local_arr' не освобождены!}}
}

//--- without_warnings.cpp
// expected-no-diagnostics
extern "C" {
    void* malloc(unsigned long size);
    void free(void* ptr);
    void* fopen(const char* filename, const char* mode);
    int fclose(void* stream);
}

void clean_malloc(int n) {
    int* p = (int*)malloc(n);
    free(p);
}

void clean_new(int n) {
    int* a = new int[n];
    delete[] a;
}

void clean_fopen(const char* path) {
    void* f = fopen(path, "r");
    fclose(f);
}

void clean_nested(int n) {
    {
        double* d = (double*)malloc(n);
        free(d);
    }
}

bool clean_branching(int n, int x) {
    int* buf = new int[n];
    if (n > x) {
        delete[] buf;
        return true;
    }
    delete[] buf;
    return false;
}