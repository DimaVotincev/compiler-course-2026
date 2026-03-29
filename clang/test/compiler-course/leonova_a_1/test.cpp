// RUN: %clang_cc1 -fcxx-exceptions -load %llvmshlibdir/leonova_a_1_ClangAST%pluginext -add-plugin add_noexcept_plugin -ast-dump %s 2>&1 | FileCheck %s

void empty() { }

void simple() { int x = 0; }

void throws() { throw 42; }

void already_noexept() noexcept { int a, b = 1; };

void bad_call() { throws(); }

void safe() noexcept;
void good_call() { safe(); }

// CHECK: FunctionDecl {{.*}} empty 'void () noexcept'
// CHECK: FunctionDecl {{.*}} simple 'void () noexcept'
// CHECK: FunctionDecl {{.*}} throws 'void ()'
// CHECK: FunctionDecl {{.*}} already_noexept 'void () noexcept'
// CHECK: FunctionDecl {{.*}} bad_call 'void ()'
// CHECK: FunctionDecl {{.*}} safe 'void () noexcept'
// CHECK: FunctionDecl {{.*}} good_call 'void () noexcept'
