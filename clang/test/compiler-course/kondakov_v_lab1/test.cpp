// RUN: %clang_cc1 -load %llvmshlibdir/kondakov_v_lab1_ClangAST%pluginext -plugin kondakov_v_lab1_plugin -fsyntax-only %s 2>&1 | FileCheck %s

void mutatePtr(int *Value) { *Value = 10; }
void rebindPtr(int *&Value) { Value = nullptr; }
void takesConstPtr(const int *Value) {
  int Var = *Value;
  (void)Var;
}

// CHECK: void ptrParamReadonly(const float{{ *\* *}}const Ptr) {
void ptrParamReadonly(float *Ptr) {
  float Var = *Ptr;
  (void)Var;
}

// CHECK: const int{{ *\* *}}const Ptr = &Value;
void localPtrReadonly() {
  int Value = 0;
  int *Ptr = &Value;
  int Var = *Ptr;
  (void)Var;
}

// CHECK: int{{ *\* *}}Ptr = &Value;
void localPtrMutatesPointee() {
  int Value = 0;
  int *Ptr = &Value;
  *Ptr = 1;
}

// CHECK-LABEL: void localPtrArithAndWrite() {
// CHECK-NOT: const int{{ *\* *}}const Ptr = &Value;
// CHECK: int{{ *\* *}}Ptr = &Value;
void localPtrArithAndWrite() {
  int Value = 0;
  int *Ptr = &Value;
  Ptr++;
  Ptr--;
  *Ptr = 1;
}

// CHECK: void refParamReadonly(const double{{ *& *}}Ref) {
void refParamReadonly(double &Ref) {
  double Var = Ref;
  (void)Var;
}

// CHECK-LABEL: void refParamMutated(double{{ *& *}}Ref) {
// CHECK-NOT: void refParamMutated(const double{{ *& *}}Ref)
void refParamMutated(double &Ref) { Ref = 4.0; }

// CHECK-LABEL: void localRefAssigned() {
// CHECK-NOT: const int{{ *& *}}Ref = Value;
// CHECK: int{{ *& *}}Ref = Value;
void localRefAssigned() {
  int Value = 10;
  int &Ref = Value;
  Ref = 100;
}

// CHECK-LABEL: void localRefIncremented() {
// CHECK-NOT: const int{{ *& *}}Ref = Value;
// CHECK: int{{ *& *}}Ref = Value;
void localRefIncremented() {
  int Value = 10;
  int &Ref = Value;
  ++Ref;
}

// CHECK-LABEL: void localPtrPreincDeref() {
// CHECK-NOT: const int{{ *\* *}}const Ptr = &Value;
// CHECK: int{{ *\* *}}Ptr = &Value;
void localPtrPreincDeref() {
  int Value = 10;
  int *Ptr = &Value;
  ++(*Ptr);
}

// CHECK-LABEL: void localPtrReassigned() {
// CHECK-NOT: const int{{ *\* *}}const Ptr = &Value;
// CHECK: int{{ *\* *}}Ptr = &Value;
void localPtrReassigned() {
  int Value = 100;
  int *Ptr = &Value;
  Ptr++;
}

// CHECK-LABEL: void ptrPassedToMutatingFunc() {
// CHECK: int{{ *\* *}}Ptr = &Value;
void ptrPassedToMutatingFunc() {
  int Value = 1;
  int *Ptr = &Value;
  mutatePtr(Ptr);
}

// CHECK-LABEL: void ptrPassedToRebindFunc() {
// CHECK: int{{ *\* *}}Ptr = &Value;
void ptrPassedToRebindFunc() {
  int Value = 1;
  int *Ptr = &Value;
  rebindPtr(Ptr);
}

// CHECK: void ptrPassedToConstPtr(const int{{ *\* *}}const Ptr) {
void ptrPassedToConstPtr(int *Ptr) { takesConstPtr(Ptr); }

// CHECK-LABEL: void addressOfPtrTaken() {
// CHECK: const int{{ *\* *}}const Ptr = &Value;
// CHECK: int{{ *\* *}}const{{ *\* *}}const PtrPtr = &Ptr;
void addressOfPtrTaken() {
  int Value = 1;
  int *Ptr = &Value;
  int **PtrPtr = &Ptr;
  (void)PtrPtr;
}
