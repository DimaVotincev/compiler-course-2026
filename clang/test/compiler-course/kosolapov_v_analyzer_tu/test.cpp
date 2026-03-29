// RUN: %clang_cc1 -load %llvmshlibdir/kosolapov_v_analyzer_tu_ClangAST%pluginext -plugin kosolapov_v_analyzer_tu -fsyntax-only %s 2>&1 | FileCheck %s

// Проверка отсутствия ложных срабатываний в корректном коде
// CHECK-NOT: Potential leak

extern "C" {
typedef struct _FILE FILE;
void *malloc(unsigned long);
void free(void *);
FILE *fopen(const char *, const char *);
int fclose(FILE *);
}

void no_leak_new_delete() {
    int* p = new int(42);
    delete p;
}

void no_leak_malloc_free() {
    int* p = (int*)malloc(sizeof(int));
    free(p);
}

void no_leak_fopen_fclose() {
    FILE* f = fopen("test.txt", "r");
    if (f) fclose(f);
}

// Проверка обнаружения утечек new
// CHECK: Potential leak of new at line [[#@LINE+2]]
void leak_new() {
    int* p = new int(10);
}

// Проверка обнаружения утечек malloc
// CHECK: Potential leak of malloc at line [[#@LINE+2]]
void leak_malloc() {
    int* p = (int*)malloc(sizeof(int));
}

// Проверка обнаружения утечек fopen
// CHECK: Potential leak of fopen at line [[#@LINE+2]]
void leak_fopen() {
    FILE* f = fopen("test.txt", "r");
}

// Проверка несохранённых выделений (unbound)
// CHECK: Potential leak of new at line [[#@LINE+2]]
void unbound_new() {
    new int(100);
}

// CHECK: Potential leak of malloc at line [[#@LINE+2]]
void unbound_malloc() {
    malloc(sizeof(int));
}

// CHECK: Potential leak of fopen at line [[#@LINE+2]]
void unbound_fopen() {
    fopen("test.txt", "r");
}

// Проверка нескольких выделений в одной функции
// CHECK: Potential leak of malloc at line [[#@LINE+3]]
void multiple_allocations() {
    int* a = new int(1);
    int* b = (int*)malloc(sizeof(int));
    delete a;
    // b не освобождён
}

// Статическая переменная (ложное срабатывание, но допустимо для упрощённого анализа)
// CHECK: Potential leak of new at line [[#@LINE+2]]
void static_var() {
    static int* p = new int(3);
}

// Ещё одна функция с утечкой
// CHECK: Potential leak of new at line [[#@LINE+2]]
void another_leak() {
    int* p = new int(4);
}

// Функция без выделений — не должно быть сообщений
void no_allocation() {
    int x = 0;
}