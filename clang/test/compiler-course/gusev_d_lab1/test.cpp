// RUN: %clang_cc1 -load %llvmshlibdir/gusev_d_lab1_ClangAST%pluginext -plugin gusev_d_lab1_plugin -DTEST_WARN -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=WARN
// RUN: %clang_cc1 -load %llvmshlibdir/gusev_d_lab1_ClangAST%pluginext -plugin gusev_d_lab1_plugin -DTEST_NOWARN -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=NOWARN --allow-empty

typedef __SIZE_TYPE__ size_t;

extern "C" void *malloc(size_t);
extern "C" void free(void *);

struct FILE;
extern "C" FILE *fopen(const char *, const char *);
extern "C" int fclose(FILE *);

#ifdef TEST_WARN
FILE *global_file = fopen("input.txt", "r");
// WARN: {{.*}}:[[@LINE-1]]:{{[0-9]+}}: warning: resource leak: file descriptor allocated with 'fopen' is not guaranteed to be released in this translation unit

void leak_new() {
  int *ptr = new int;
  // WARN: {{.*}}:[[@LINE-1]]:{{[0-9]+}}: warning: resource leak: memory allocated with 'new' is not guaranteed to be released in this translation unit
}

void leak_malloc() {
  void *buffer = malloc(sizeof(int));
  // WARN: {{.*}}:[[@LINE-1]]:{{[0-9]+}}: warning: resource leak: memory allocated with 'malloc' is not guaranteed to be released in this translation unit
}

void leak_expression_statements() {
  new int;
  // WARN: {{.*}}:[[@LINE-1]]:{{[0-9]+}}: warning: resource leak: memory allocated with 'new' is not guaranteed to be released in this translation unit

  malloc(sizeof(int));
  // WARN: {{.*}}:[[@LINE-1]]:{{[0-9]+}}: warning: resource leak: memory allocated with 'malloc' is not guaranteed to be released in this translation unit

  fopen("input.txt", "r");
  // WARN: {{.*}}:[[@LINE-1]]:{{[0-9]+}}: warning: resource leak: file descriptor allocated with 'fopen' is not guaranteed to be released in this translation unit
}

void mismatched_release() {
  int *ptr = new int;
  free(ptr);
  // WARN: {{.*}}:[[@LINE-2]]:{{[0-9]+}}: warning: resource leak: memory allocated with 'new' is not guaranteed to be released in this translation unit
}

void overwrite_then_delete() {
  int *ptr = new int;
  ptr = nullptr;
  delete ptr;
  // WARN: {{.*}}:[[@LINE-3]]:{{[0-9]+}}: warning: resource leak: memory allocated with 'new' is not guaranteed to be released in this translation unit
}

void overwrite_with_new_then_delete_latest() {
  int *ptr = new int;
  ptr = new int;
  delete ptr;
  // WARN: {{.*}}:[[@LINE-3]]:{{[0-9]+}}: warning: resource leak: memory allocated with 'new' is not guaranteed to be released in this translation unit
}
#endif

#ifdef TEST_NOWARN
// NOWARN-NOT: warning:

int *global_ptr = nullptr;
void *global_buffer = nullptr;
FILE *global_file_ok = nullptr;

int *initialized_global_ptr = new int;
void *initialized_global_buffer = malloc(sizeof(int));
FILE *initialized_global_file = fopen("input.txt", "r");

void allocate_globals() {
  global_ptr = new int;
  global_buffer = malloc(sizeof(int));
  global_file_ok = fopen("input.txt", "r");
}

void free_globals() {
  delete global_ptr;
  free(global_buffer);
  fclose(global_file_ok);
}

void free_initialized_globals() {
  delete initialized_global_ptr;
  free(initialized_global_buffer);
  fclose(initialized_global_file);
}

void direct_releases() {
  delete (new int);
  free(malloc(sizeof(int)));
  fclose(fopen("input.txt", "r"));
}

void local_releases() {
  int *ptr = new int;
  void *buffer = malloc(sizeof(int));
  FILE *file = fopen("input.txt", "r");

  delete ptr;
  free(buffer);
  fclose(file);
}

void parenthesized_local_releases() {
  int *ptr = new int;
  void *buffer = malloc(sizeof(int));
  FILE *file = fopen("input.txt", "r");

  delete (ptr);
  free((buffer));
  fclose((file));
}

void reuse_owner_after_release() {
  int *ptr = new int;
  delete ptr;

  ptr = new int;
  delete ptr;
}

void array_release() {
  int *buffer = new int[4];
  delete[] buffer;
}
#endif
