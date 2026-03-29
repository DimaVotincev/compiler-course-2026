// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/nikolaev_d_lab1_ClangAST%pluginext -add-plugin nikolaev_d_analyzer_plugin -fsyntax-only -verify %t/leak_tests.cpp
// RUN: %clang_cc1 -load %llvmshlibdir/nikolaev_d_lab1_ClangAST%pluginext -add-plugin nikolaev_d_analyzer_plugin -fsyntax-only -verify %t/correct_tests.cpp

//--- leak_tests.cpp
extern "C" {
  void* malloc(unsigned long size);
  void free(void* ptr);
  void* fopen(const char* filename, const char* mode);
}

void test_malloc_no_return() {
  int* p = (int*)malloc(100); // expected-warning {{malloc resource 'p' is not freed}}
}

void test_new_no_return() {
  int* p = new int(67); // expected-warning {{memory resource 'p' is not freed}}
}

void test_fopen_no_return() {
  void* f = fopen("test.txt", "r"); // expected-warning {{file resource 'f' is not freed}}
}

void test_multiple_vars() {
  int* p1 = (int*)malloc(100);
  int* p2 = (int*)malloc(200); // expected-warning {{malloc resource 'p2' is not freed}}
  int* p3 = new int(67);
  free(p1);
  delete p3;
}

void test_static() {
  static int* static_leak = (int*)malloc(500); // expected-warning {{malloc resource 'static_leak' is not freed}}
}

void test_new_array() {
  int* arr = new int[30]; // expected-warning {{memory resource 'arr' is not freed}}
}

int* test_new_return(int sz) {
  int* p = new int[sz];
  return p; // expected-warning {{memory resource 'p' may escape via return}}
}

void* test_fopen_return() {
  void* f = fopen("file.txt", "r");
  return f; // expected-warning {{file resource 'f' may escape via return}}
}

void test_binary_operator() {
  int* t;
  t = (int*)malloc(100); // expected-warning {{malloc resource 't' is not freed}}
}

void test_binary_operator_new() {
  int* q;
  q = new int(67); // expected-warning {{memory resource 'q' is not freed}}
}

//--- correct_tests.cpp
// expected-no-diagnostics

extern "C" {
  void* malloc(unsigned long size);
  void free(void* ptr);
  void* fopen(const char* filename, const char* mode);
  int fclose(void* stream);
}

void test_fclose() {
  void* f = fopen("test.txt", "r");
  fclose(f);
}

void test_free() {
  int* p = (int*)malloc(100);
  free(p);
}

void test_delete() {
  int* p = new int(67);
  delete p;
}

void test_delete_array() {
  int* p = new int[10];
  delete[] p;
}

bool test_if(int sz) {
  int* p = new int[sz];
  if (sz > 10) {
    delete[] p;
    return true;
  }
  delete[] p;
  return false;
}
