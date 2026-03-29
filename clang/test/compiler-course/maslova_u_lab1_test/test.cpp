// RUN: %clang_cc1 -load %llvmshlibdir/maslova_u_lab1_ClangAST%pluginext -plugin maslova_analyzer -fsyntax-only %s 2>&1 | FileCheck %s

extern "C" {
    void* malloc(unsigned long size);
    void free(void* ptr);
    void* fopen(const char* filename, const char* mode);
    int fclose(void* stream);
}

// CHECK: warning: MaslovaAnalyzer: Resource 'memory (malloc)' for variable 'p1' might not be released
void test_malloc_leak() {
    void* p1 = malloc(10);
}

// CHECK-NOT: warning: MaslovaAnalyzer: Resource 'memory (malloc)' for variable 'p2'
void test_malloc_ok() {
    void* p2 = malloc(20);
    free(p2);
}

// CHECK: warning: MaslovaAnalyzer: Resource 'memory (new)' for variable 'p3' might not be released
void test_new_leak() {
    int* p3 = new int(5);
}

// CHECK-NOT: warning: MaslovaAnalyzer: Resource 'memory (new)' for variable 'p4'
void test_new_ok() {
    int* p4 = new int;
    delete p4;
}

// CHECK: warning: MaslovaAnalyzer: Resource 'file (fopen)' for variable 'f1' might not be released
void test_fopen_leak() {
    void* f1 = fopen("test.txt", "r");
}

// CHECK-NOT: warning: MaslovaAnalyzer: Resource 'file (fopen)' for variable 'f2'
void test_fopen_ok() {
    void* f2 = fopen("test.txt", "w");
    fclose(f2);
}

// CHECK: warning: MaslovaAnalyzer: Resource 'memory (malloc)' for variable 'p5' might not be released
void test_return_leak(int x) {
    void* p5 = malloc(100);
    if (x > 0) {
        return; 
    }
    free(p5);
}

// CHECK: warning: MaslovaAnalyzer: Resource 'memory (malloc)' for variable 'p_leak' might not be released
void test_multiple_resources() {
    void* p_ok = malloc(10);
    void* p_leak = malloc(20);
    free(p_ok);
}

// CHECK: warning: MaslovaAnalyzer: Resource 'memory (new)' for variable 'p6' might not be released
void test_assignment_leak() {
    int* p6;
    p6 = new int[10];
}

// CHECK: warning: MaslovaAnalyzer: Resource 'memory (malloc)' for variable 'p7' might not be released
void test_nested_scope() {
    {
        void* p7 = malloc(50);
    }
}
