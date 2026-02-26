
// RUN: %clang_cc1 -load %llvmshlibdir/VotincevDAnalyzer_Votincev_D_FIIT3_ClangAST%pluginext -plugin votincev_d_analyzerplugin -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK: FunctionDecl {{0x[0-9a-fA-F]+}} <{{.*}}> col:20 isEven 'bool (int) noexcept'
// CHECK-NEXT: |-ParmVarDecl {{0x[0-9a-fA-F]+}} <col:27, col:31> col:31 used value 'int'
// CHECK-NEXT: |-CompoundStmt {{0x[0-9a-fA-F]+}} <col:47, col:72>
// CHECK-NEXT: | `-ReturnStmt {{0x[0-9a-fA-F]+}} <col:49, col:69>
// CHECK-NEXT: |   `-BinaryOperator {{0x[0-9a-fA-F]+}} <col:56, col:69> 'bool' '=='
// CHECK-NEXT: |     |-BinaryOperator {{0x[0-9a-fA-F]+}} <col:56, col:64> 'int' '%'
// CHECK-NEXT: |     | |-ImplicitCastExpr {{0x[0-9a-fA-F]+}} <col:56> 'int' <LValueToRValue>
// CHECK-NEXT: |     | | `-DeclRefExpr {{0x[0-9a-fA-F]+}} <col:56> 'int' lvalue ParmVar {{0x[0-9a-fA-F]+}} 'value' 'int'
// CHECK-NEXT: |     | `-IntegerLiteral {{0x[0-9a-fA-F]+}} <col:64> 'int' 2
// CHECK-NEXT: |     `-IntegerLiteral {{0x[0-9a-fA-F]+}} <col:69> 'int' 0
// CHECK-NEXT: `-WarnUnusedResultAttr {{0x[0-9a-fA-F]+}} <col:3> nodiscard ""


// запускается clang для ЭТОГО файла

// этот код преобразуется в AST
// его мы проверяем комментариями + пометка чек
// если все совпало == тест отработал верно

// чтобы получить чеки, нужно запустить команду:
/*

/mnt/d/CompilersRepo/compiler-course-2026/build/bin/clang -cc1 \
-load /mnt/d/CompilersRepo/compiler-course-2026/build/lib/VotincevDAnalyzer_Votincev_D_FIIT3_ClangAST.so \
-plugin votincev_d_analyzerplugin \
-fsyntax-only \
/mnt/d/CompilersRepo/compiler-course-2026/clang/test/compiler-course/votincev_d_analyzer/test.cpp

*/
// после - в консоли появляется дерево AST для данного файла

// ниже пишется код, который тестируется с помощью clang


// чтобы clang видел malloc, free, fopen, fclose
extern "C" {
    void* malloc(unsigned long size);
    void free(void* ptr);
    void* fopen(const char* filename, const char* mode);
    int fclose(void* stream);
}


[[nodiscard]] bool isEven(int value) noexcept { 
    int n = 10;

    // leaks:
    int* mem_leak1 = (int*) malloc(n*sizeof(int));
    int* mem_leak2 = new int[n];
    void* mem_leak3 = fopen("test.cpp","r");

    // non leaks:
    int* mem1 = (int*) malloc(n*sizeof(int));
    int* mem2 = new int[n];
    void* mem3 = fopen("test.cpp","r");


    free(mem1);
    delete[] mem2;
    fclose(mem3);


    return false;
}