// RUN: %clang_cc1 -load %llvmshlibdir/eremin_v_lab_1_ClangAST%pluginext -plugin eremin_v_lab_1_resource_checker -fsyntax-only %s 2>&1 | FileCheck %s

typedef unsigned long size_t;
extern "C" void* malloc(size_t);
extern "C" void free(void*);

struct FILE;
extern "C" FILE* fopen(const char*, const char*);
extern "C" int fclose(FILE*);



void testNewLeak() {
    // CHECK-LABEL: testNewLeak
    // CHECK-DAG: Warning: resource 'new' not released at {{.*}}:[[@LINE+1]]
    int* a = new int(42);
}

// CHECK-LABEL: testNewDelete
// CHECK-NOT: Warning: resource 'new' not released
void testNewDelete() {
    int* a = new int(5);
    delete a;
}

void testNewArrayLeak() {
    // CHECK-LABEL: testNewArrayLeak
    // CHECK-DAG: Warning: resource 'new[]' not released at {{.*}}:[[@LINE+1]]
    int* arr = new int[10];
}

// CHECK-LABEL: testNewArrayDelete
// CHECK-NOT: Warning: resource 'new[]' not released
void testNewArrayDelete() {
    int* arr = new int[3];
    delete[] arr;
}



void testMallocLeak() {
    // CHECK-LABEL: testMallocLeak
    // CHECK-DAG: Warning: resource 'malloc' not released at {{.*}}:[[@LINE+1]]
    int* p = (int*)malloc(100);
}

// CHECK-LABEL: testMallocFree
// CHECK-NOT: Warning: resource 'malloc' not released
void testMallocFree() {
    int* p = (int*)malloc(50);
    free(p);
}



void testFileLeak() {
    // CHECK-LABEL: testFileLeak
    // CHECK-DAG: Warning: resource 'fopen' not released at {{.*}}:[[@LINE+1]]
    FILE* f = fopen("data.txt", "r");
}

// CHECK-LABEL: testFileClose
// CHECK-NOT: Warning: resource 'fopen' not released
void testFileClose() {
    FILE* f = fopen("ok.txt", "r");
    fclose(f);
}



void testAssignmentLeak() {
    int* ptr;
    // CHECK-LABEL: testAssignmentLeak
    // CHECK-DAG: Warning: resource 'new' not released at {{.*}}:[[@LINE+1]]
    ptr = new int(777);
}

void testAssignmentArrayLeak() {
    int* arr;
    // CHECK-LABEL: testAssignmentArrayLeak
    // CHECK-DAG: Warning: resource 'new[]' not released at {{.*}}:[[@LINE+1]]
    arr = new int[5];
}



void testScopeLeaks() {
    // CHECK-LABEL: testScopeLeaks

    if (true) {
        // CHECK-DAG: Warning: resource 'new' not released at {{.*}}:[[@LINE+1]]
        int* a = new int(100);
    }

    for (int i = 0; i < 2; ++i) {
        // CHECK-DAG: Warning: resource 'new' not released at {{.*}}:[[@LINE+1]]
        int* p = new int(i);
    }
}



template<typename T>
T* createLeakyArray(int n) {
    // CHECK-LABEL: createLeakyArray
    // CHECK-DAG: Warning: resource 'new[]' not released at {{.*}}:[[@LINE+1]]
    T* arr = new T[n];
    return arr;
}

void testTemplateLeaks() {
    auto* a = createLeakyArray<int>(5);
    auto* b = createLeakyArray<double>(3);
}



void testCleanMemory() {
    // CHECK-LABEL: testCleanMemory
    // CHECK-NOT: Warning: resource 'new'
    // CHECK-NOT: Warning: resource 'new[]'
    // CHECK-NOT: Warning: resource 'malloc'
    // CHECK-NOT: Warning: resource 'fopen'

    int* a = new int(10);
    delete a;

    int* arr = new int[3];
    delete[] arr;

    int* m = (int*)malloc(10);
    free(m);

    FILE* f = fopen("clean.txt", "r");
    fclose(f);
}



int main() {
    testNewLeak();
    testNewDelete();
    testNewArrayLeak();
    testNewArrayDelete();
    testMallocLeak();
    testMallocFree();
    testFileLeak();
    testFileClose();
    testAssignmentLeak();
    testAssignmentArrayLeak();
    testScopeLeaks();
    testTemplateLeaks();
    testCleanMemory();
}