
// RUN: %clang_cc1 -load %llvmshlibdir/votincev_d_analyzer_ClangAST%pluginext -plugin votincev_d_analyzerplugin -fsyntax-only %s 2>&1 | FileCheck %s

// этот код преобразуется в AST
// его мы проверяем комментариями + пометка чек
// если все совпало == тест отработал верно


// ниже пишется код, который тестируется с помощью clang


// чтобы clang видел malloc, free, fopen, fclose
extern "C" {
    void* malloc(unsigned long size);
    void free(void* ptr);
    void* fopen(const char* filename, const char* mode);
    int fclose(void* stream);
}


int n = 10;



// global scope

// non leak
int* global_mem1 = (int*) malloc(n*sizeof(int));

// leak
int* global_mem_leak1 = (int*) malloc(n*sizeof(int));



[[nodiscard]] bool test1(int value) noexcept { 
    

    // leaks:
    int* mem_leak1 = (int*) malloc(n*sizeof(int));
    int* mem_leak2 = new int[n];
    void* mem_leak3 = fopen("test.cpp","r");

    // non leaks:
    int* mem1 = (int*) malloc(n*sizeof(int));
    int* mem2 = new int[n];
    void* mem3 = fopen("test.cpp","r");


    // non leak
    {
        double* mem4;
        mem4 = (double*) malloc(n*sizeof(double));
        delete mem4;
    }

    // leak
    {
        double* mem_leak4;
        mem_leak4 = (double*) malloc(n*sizeof(double));
    }

    // non leak
    {
        {
            char* mem5;
            mem5 = (char*) malloc(n*sizeof(char));
            delete mem5;
        }
    }

    // leak
    {
        {
            char* mem_leak5;
            mem_leak5 = (char*) malloc(n*sizeof(char));
        }
    }



    free(mem1);
    delete[] mem2;
    fclose(mem3);
    delete global_mem1;




    

    // leaks:
    int* mem_leak6 = (int*) malloc(n*sizeof(int));

    // non leaks:
    int* mem6 = (int*) malloc(n*sizeof(int));

    // condition leak
    if(value < 5) {
        delete[] mem6;
        return false;
        // CHECK: warning: Ресурс для переменной 'global_mem_leak1' может быть не освобожден (не гарантированное освобождение при return)!
        // CHECK: warning: Ресурс для переменной 'mem_leak1' может быть не освобожден (не гарантированное освобождение при return)!
        // CHECK: warning: Ресурс для переменной 'mem_leak2' может быть не освобожден (не гарантированное освобождение при return)!
        // CHECK: warning: Ресурс для переменной 'mem_leak3' может быть не освобожден (не гарантированное освобождение при return)!
        // CHECK: warning: Ресурс для переменной 'mem_leak4' может быть не освобожден (не гарантированное освобождение при return)!
        // CHECK: warning: Ресурс для переменной 'mem_leak5' может быть не освобожден (не гарантированное освобождение при return)!
        // CHECK: warning: Ресурс для переменной 'mem_leak6' может быть не освобожден (не гарантированное освобождение при return)!
    }

    // если бы не было разветвления - warning появились бы у return ниже


    delete[] mem6;
    return false;
}



[[nodiscard]] bool test2(int value) noexcept { 
    

    // leaks:
    int* mem_leak1 = (int*) malloc(n*sizeof(int));
    // CHECK: warning: Память или ресурс для переменной 'mem_leak1' не освобождены!

    int* mem_leak2 = new int[n];
    // CHECK: warning: Память или ресурс для переменной 'mem_leak2' не освобождены!

    void* mem_leak3 = fopen("test.cpp","r");
    // CHECK: warning: Память или ресурс для переменной 'mem_leak3' не освобождены!

    // non leaks:
    int* mem1 = (int*) malloc(n*sizeof(int));
    int* mem2 = new int[n];
    void* mem3 = fopen("test.cpp","r");

    free(mem1);
    delete[] mem2;
    fclose(mem3);

}