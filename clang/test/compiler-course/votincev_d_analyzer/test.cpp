// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/votincev_d_analyzer_ClangAST%pluginext -plugin votincev_d_analyzerplugin -fsyntax-only -verify %t/with_warnings.cpp
// RUN: %clang_cc1 -load %llvmshlibdir/votincev_d_analyzer_ClangAST%pluginext -plugin votincev_d_analyzerplugin -fsyntax-only -verify %t/without_warnings.cpp

//--- with_warnings.cpp
extern "C" {
    void* malloc(unsigned long size);
    void* fopen(const char* filename, const char* mode);
}

int* g_leak = (int*)malloc(100); 

int* test_malloc_return(int sz) {
    int* p = (int*)malloc(sz);
    return p; // expected-warning {{Ресурс для переменной 'g_leak' может быть не освобожден (не гарантированное освобождение при return)!}} expected-warning {{Ресурс для переменной 'p' может быть не освобожден (не гарантированное освобождение при return)!}}
}

int* test_new_return(int sz) {
    int* p = new int[sz];
    return p; // expected-warning {{Ресурс для переменной 'p' может быть не освобожден (не гарантированное освобождение при return)!}}
}

void* test_fopen_return(const char* name) {
    void* f = fopen(name, "r");
    return f; // expected-warning {{Ресурс для переменной 'f' может быть не освобожден (не гарантированное освобождение при return)!}}
}

void test_branching_leak(int sz,int value) {
    int* p = new int[sz];
    if (value < 5) {
        return; // expected-warning {{Ресурс для переменной 'p' может быть не освобожден (не гарантированное освобождение при return)!}}
    }
    delete[] p;
}

void test_nested_return(int sz) {
    {
        {
            int* p_nested = (int*)malloc(sz);
        }
    }
    return; // expected-warning {{Ресурс для переменной 'p_nested' может быть не освобожден (не гарантированное освобождение при return)!}}
}

void test_nested_no_return(int sz) {
    {
        {
            int* p_nested = (int*)malloc(sz); // expected-warning {{Память или ресурс для переменной 'p_nested' не освобождены!}}
        }
    }
}

void test_malloc_no_return(int sz) {
    int* p = (int*)malloc(sz); // expected-warning {{Память или ресурс для переменной 'p' не освобождены!}}
}

void test_new_array_no_return(int sz) {
    int* p = new int[sz]; // expected-warning {{Память или ресурс для переменной 'p' не освобождены!}}
}



//--- without_warnings.cpp
// expected-no-diagnostics
extern "C" {
    void* malloc(unsigned long size);
    void free(void* ptr);
    void* fopen(const char* filename, const char* mode);
    int fclose(void* stream);
}

void test_clean_malloc(int sz) {
    int* p = (int*)malloc(sz);
    free(p);
}

void test_clean_new(int sz) {
    int* p = new int[sz];
    delete[] p;
}

void test_clean_fopen(const char* name) {
    void* f = fopen(name, "r");
    fclose(f);
}

void test_clean_nested(int sz) {
    {
        double* p = (double*)malloc(sz);
        free(p);
    }
}

bool test_clean_branching(int sz, int value) {
    int* p = new int[sz];
    if (sz > value) {
        delete[] p;
        return true;
    }
    delete[] p;
    return false;
}
