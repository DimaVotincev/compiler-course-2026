// RUN: %clang_cc1 -load %llvmshlibdir/fatehov_k_lab1_ClangAST%pluginext -plugin fatehov_resource_leak -fsyntax-only %s 2>&1 | FileCheck %s

typedef unsigned long size_t;
extern "C" void* malloc(size_t);
extern "C" void* calloc(size_t, size_t);
extern "C" void free(void*);

struct FILE;
extern "C" FILE* fopen(const char*, const char*);
extern "C" int fclose(FILE*);

namespace std {
    template <typename T>
    class unique_ptr {
        T* ptr;
    public:
        explicit unique_ptr(T* p) : ptr(p) {}
        ~unique_ptr() { delete ptr; }
    };
}

void testNewLeaks() {
    // CHECK-DAG: [LEAK DETECTED] Variable 'ptr1' allocated with operator new at {{.*}}:[[@LINE+1]] has no corresponding deallocation
    int* ptr1 = new int(42);           
    int* ptr2 = new int(100);
    delete ptr2;                        
    
    // CHECK-DAG: [LEAK DETECTED] Variable 'ptr3' allocated with operator new at {{.*}}:[[@LINE+1]] has no corresponding deallocation
    int* ptr3 = new int[10];            
    // delete[] ptr3;
}

void testMallocLeaks() {
    // CHECK-DAG: [LEAK DETECTED] Variable 'mem1' allocated with malloc/calloc at {{.*}}:[[@LINE+1]] has no corresponding deallocation
    int* mem1 = (int*)malloc(sizeof(int) * 5);    
    // CHECK-DAG: [LEAK DETECTED] Variable 'mem2' allocated with malloc/calloc at {{.*}}:[[@LINE+1]] has no corresponding deallocation
    int* mem2 = (int*)calloc(10, sizeof(int));    
    
    int* mem3 = (int*)malloc(sizeof(int) * 3);
    free(mem3);
    
    // CHECK-DAG: [LEAK DETECTED] Variable 'mem4' allocated with malloc/calloc at {{.*}}:[[@LINE+1]] has no corresponding deallocation
    char* mem4 = (char*)malloc(100);                
}

void testFileLeaks() {
    // CHECK-DAG: [LEAK DETECTED] Variable 'file1' allocated with fopen at {{.*}}:[[@LINE+1]] has no corresponding deallocation
    FILE* file1 = fopen("data1.txt", "r");        
    
    FILE* file2 = fopen("data2.txt", "w");
    fclose(file2);
    
    // CHECK-DAG: [LEAK DETECTED] Variable 'file3' allocated with fopen at {{.*}}:[[@LINE+1]] has no corresponding deallocation
    FILE* file3 = fopen("data3.txt", "a");         
}

void testAssignmentLeaks() {
    int* latePtr;
    // CHECK-DAG: [LEAK DETECTED] Variable 'latePtr' allocated with operator new at {{.*}}:[[@LINE+1]] has no corresponding deallocation
    latePtr = new int(777);                         
    
    FILE* lateFile;
    // CHECK-DAG: [LEAK DETECTED] Variable 'lateFile' allocated with fopen at {{.*}}:[[@LINE+1]] has no corresponding deallocation
    lateFile = fopen("late.txt", "r");              
    
    int* goodPtr;
    goodPtr = new int(888);
    delete goodPtr;
}

void testScopeLeaks() {
    if (true) {
        // CHECK-DAG: [LEAK DETECTED] Variable 'scopePtr' allocated with operator new at {{.*}}:[[@LINE+1]] has no corresponding deallocation
        int* scopePtr = new int(555);                
    }
    
    for (int i = 0; i < 3; ++i) {
        // CHECK-DAG: [LEAK DETECTED] Variable 'loopPtr' allocated with operator new at {{.*}}:[[@LINE+1]] has no corresponding deallocation
        int* loopPtr = new int(i);                   
    }
}

void testCleanCode() {
    int* cleanPtr = new int(111);
    delete cleanPtr;
    
    FILE* cleanFile = fopen("clean.txt", "r");
    if (cleanFile) {
        fclose(cleanFile);
    }
    
    int* cleanMem = (int*)calloc(5, sizeof(int));
    free(cleanMem);
    
    std::unique_ptr<int> smartPtr(new int(222));
}

template<typename T>
T* createLeakyArray(int size) {
    // CHECK-DAG: [LEAK DETECTED] Variable 'arr' allocated with operator new at {{.*}}:[[@LINE+1]] has no corresponding deallocation
    T* arr = new T[size];                             
    return arr;
}

void testTemplateLeaks() {
    auto* leakyInts = createLeakyArray<int>(10);
    auto* leakyDoubles = createLeakyArray<double>(5);
}

int main() {
    testNewLeaks();
    testMallocLeaks();
    testFileLeaks();
    testAssignmentLeaks();
    testScopeLeaks();
    testCleanCode();
    testTemplateLeaks();
    return 0;
}