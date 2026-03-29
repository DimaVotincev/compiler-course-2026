// RUN: %clang_cc1 -load %llvmshlibdir/chyokotov_alexey_FI2_ClangAST%pluginext -add-plugin chyokotov_a_analyzer_plugin -fsyntax-only -verify %s
extern "C" {
  void* malloc(unsigned long size);
  void free(void* ptr);
  void* fopen(const char* filename, const char* mode);
  int fclose(void* stream);
}

void test_malloc_no_return() {
  int* p = (int*)malloc(100); // expected-warning {{memory leak: 'p'}}
}

void test_new_no_return() {
  int* p = new int(42); // expected-warning {{memory leak: 'p'}}
}

void test_fopen_no_return() {
  void* f = fopen("test.txt", "r"); // expected-warning {{memory leak: 'f'}}
}

void test_free() {
  int* p = (int*)malloc(100);
  free(p);
}

void test_fclose() {
  void* f = fopen("test.txt", "r");
  fclose(f);
}

void test_delete() {
  int* p = new int(42);
  delete p;
}

void test_multiple_vars() {
  int* p1 = (int*)malloc(100);
  int* p2 = (int*)malloc(200); // expected-warning {{memory leak: 'p2'}}
  int* p3 = new int(42);
  free(p1);
  delete p3;
}

void test_static() {
  static int* static_leak = (int*)malloc(500); // expected-warning {{memory leak: 'static_leak'}}
}

void test_new_array() {
  int* arr = new int[50]; // expected-warning {{memory leak: 'arr'}}
}

int* test_new_return(int sz) {
  int* p = new int[sz];
  return p; // expected-warning {{resource leak: 'p' may not be freed (no guaranteed deallocation on return)}}
}

void* test_fopen_return() {
  void* f = fopen("test.txt", "r");
  return f; // expected-warning {{resource leak: 'f' may not be freed (no guaranteed deallocation on return)}}
}

void test_binary_operator() {
  int* t;
  t = (int*)malloc(100); // expected-warning {{memory leak: 't'}}
}

void test_binary_operator_new() {
  int* q;
  q = new int(42); // expected-warning {{memory leak: 'q'}}
}

void test_delete_array() {
  int* p = new int[10];
  delete[] p;
}

bool test_branching(int sz) {
  int* p = new int[sz];
  if (sz > 10) {
    delete[] p;
    return true;
  }
  delete[] p;
  return false;
}

bool test_branching_with_one(int sz) {
  int* p = new int[sz];
  if (sz > 10) {
    return true; // expected-warning {{resource leak: 'p' may not be freed (no guaranteed deallocation on return)}}
  }
  delete[] p;
  return false; 
}