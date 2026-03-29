// RUN: %clang_cc1 -fcxx-exceptions -fexceptions -load %llvmshlibdir/smyshlaev_a_lab1_ClangAST.so -add-plugin smyshlaev_a_lab1_plugin -ast-dump %s 2>&1 | FileCheck %s

// Тест 1: Не должна стать noexcept
// CHECK: FunctionDecl {{.*}} f 'int (int, int)'
// CHECK-NOT: noexcept
int f(int a, int b) {
    if (a>0) throw 0; else return a + b; 
}

// Тест 2: Должна стать noexcept
// CHECK: FunctionDecl {{.*}} g 'int () noexcept'
int g() {
    int c = 3 + 4;
    return c;
}

// Тест 3: Вызов noexcept функции должен позволить функции стать noexcept
// CHECK: FunctionDecl {{.*}} callg 'int () noexcept'
int callg() {
    int j = g();
    return j;
}

// Тест 4: Вызов функции, которая кидает throw, должен оставить функцию обычной
// CHECK: FunctionDecl {{.*}} callf 'int (int, int)'
// CHECK-NOT: noexcept
int callf(int a, int b) {
    int c = f(a,b);
    return c;
}

// Тест 5: Функция, которая уже noexcept, не должна меняться
// CHECK: FunctionDecl {{.*}} k 'int () noexcept'
int k() noexcept {
    return 3;
}

// Тест 6: Вызов функции, которая уже noexcept, делает вызывающую тоже noexcept
// CHECK: FunctionDecl {{.*}} callk 'int () noexcept'
int callk() {
    return k();
}

// Тест 7: Объявление без тела не должно меняться
// CHECK: FunctionDecl {{.*}} l 'int ()'
int l();