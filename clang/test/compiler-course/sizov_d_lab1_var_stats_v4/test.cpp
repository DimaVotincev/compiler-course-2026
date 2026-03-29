// RUN: split-file %s %t
// RUN: %clang_cc1 -load %llvmshlibdir/SizovDLab1VarStatsV4Plugin_Sizov_D_FIIT2_ClangAST%pluginext -plugin SizovDLab1VarStatsV4Plugin_Sizov_D_FIIT2_ClangAST -fsyntax-only %t/with_vars.cpp 2>&1 | FileCheck %s --check-prefix=STATS
// RUN: %clang_cc1 -load %llvmshlibdir/SizovDLab1VarStatsV4Plugin_Sizov_D_FIIT2_ClangAST%pluginext -plugin SizovDLab1VarStatsV4Plugin_Sizov_D_FIIT2_ClangAST -fsyntax-only %t/without_vars.cpp 2>&1 | FileCheck %s --check-prefix=EMPTY

// STATS: Variable stats for TU:
// STATS-NEXT: Global objects: 2
// STATS-NEXT: Local variables: 3
// STATS-NEXT: Static variables: 4
// STATS-NEXT: Function parameters: 4

// EMPTY: Variable stats for TU:
// EMPTY-NEXT:   (no variables found)

//--- with_vars.cpp
int GlobalA = 1;
int GlobalB = 2;
static int StaticGlobal = 3;
extern int ExternOnly;

class C {
public:
  static int StaticMember;
};
int C::StaticMember = 42;

int foo(int x, int y) {
  int LocalA = x + y;
  int BlockLocal = LocalA;
  static int StaticLocal = 0;
  {
    static int StaticInBlock = 0;
    BlockLocal += StaticInBlock;
  }
  return BlockLocal + StaticLocal;
}

int bar(int z) {
  int LocalB = z;
  return LocalB;
}

int proto(int q);

int baz(int p) { return p; }

//--- without_vars.cpp
int NoVarsHere() { return 0; }
