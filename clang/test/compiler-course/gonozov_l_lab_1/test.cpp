// RUN: %clang_cc1 -load %llvmshlibdir/VariablesStatisticsPlugin_Gonozov_Leonid_FIIT3_ClangAST%pluginext -plugin variables_statistics_plugin -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK: Total count: 18
// CHECK-NEXT: Global variables: 1
// CHECK-NEXT: Local variables: 5
// CHECK-NEXT: Static variables: 6
// CHECK-NEXT: Function parameters: 6

bool isEven(int value) noexcept { return value % 2 == 0; } // Function parameters++ (1)

template<typename F>
static F A(F arg1) { // Function parameters++ (2)
    static F cache; // Static variables++ (1)
    return cache;
}

static int B(int x) { // Function parameters++ 3
    int result = x * 3; // Local variables++ 1
    return result;
}

class Base
{
    double f;
    public:
        Base(double f_): f(f_) {} // Function parameters++ 4
};

extern int g;
int g = 2; 

namespace {
    extern int a1;    // Global variables++ 2
    static int a2;    // Static variables++ 2
}

namespace {
    int a1;
}

namespace Fi 
{
    extern int n; 
    static int f; // Static variables++ 3
}

namespace Fe
{
    extern int n; 
    static int f; // Static variables++ (разные namespace) 4
}

double f1 = 2.6; // Global variables++ 5
static double f2 = 2.6; // Static variables++ 5

int main(int argc, char** argv) { // Function parameters += 2:  5, 6

    Base C(3.5); // Local variables++ 2
    int res = B(f1); // Local variables++ 3
    int e = 0; // Local variables++ 4
    constexpr int d = 100;        // Local variables++ 5
    static constexpr float ff = 3.14f;// Static variables++ 6 
    
    return 0;
}

