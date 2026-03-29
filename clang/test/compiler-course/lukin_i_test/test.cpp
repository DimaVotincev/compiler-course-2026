// RUN: %clang_cc1 -load %llvmshlibdir/VariableStatisticsPlugin_Lukin_Ivan_FIIT3_ClangAST%pluginext -plugin VariableStatisticsPlugin -fsyntax-only %s 2>&1 | FileCheck %s

int global1 = 0;

static int static1 = 0;

class Example{
    static int static2;
    int nonVisible;
};

void foo1()
{
    int local1 = 0;
    static int static3 = 0;
    double local2 = 0.0;
}

double global2 = 0.0;

void foo2(int param1, int param2)
{
    int local3 = 0;
}

namespace
{
    static int static4;
    int global3;
}

void foo3(double param3, unsigned param4)
{
    for(int local4 = 0; local4 < 4; local4++);
}


// CHECK: Statistics
// CHECK-NEXT: Global objects: 3
// CHECK-NEXT: Local variables: 4
// CHECK-NEXT: Static variables: 4
// CHECK-NEXT: Params: 4