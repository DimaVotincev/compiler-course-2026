// RUN: %clang_cc1 -fcxx-exceptions -fexceptions -load %llvmshlibdir/NoexceptSpecificatorPlugin_Kutergin_Valentin_FIIT3_ClangAST%pluginext -add-plugin kutergin_valentin_3823b1fi3 -ast-dump %s 2>&1 | FileCheck %s

// Тест 1: Пустая функция должна стать noexcept
// CHECK: FunctionDecl {{.*}} func1 'void () noexcept'
void func1() {}

// Тест 2: Функция с throw НЕ должна стать noexcept
// CHECK: FunctionDecl {{.*}} func2 'void ()'
// CHECK-NOT: noexcept
void func2() { 
    throw 1;
}

// Тест 3: Вызов опасной функции
// CHECK: FunctionDecl {{.*}} func3 'void ()'
// CHECK-NOT: noexcept
void func3() {
    func2();
}

// Тест 4: Функция с аргументами и без вызова исключений должна стать Noexcept
// CHECK: FunctionDecl {{.*}} func4 'int (int, int) noexcept'
int func4(int a, int b) {
    return a + b;
}