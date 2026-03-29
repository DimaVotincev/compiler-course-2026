// RUN: %clang_cc1 -load %llvmshlibdir/VariablesStatisticPlugin_Khruev_Anton_FIIT1_ClangAST%pluginext -plugin variables_statistic -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK: Total count : 29
// CHECK-NEXT: Global variables : 4
// CHECK-NEXT: Static variables : 11
// CHECK-NEXT: Local variables  : 6
// CHECK-NEXT: Function params  : 8


static bool is_active = false;        // global var: 1
static double global_val = 3.14;      // global var: 2
static char static_flag = 'A'; // static var: 1

static short g1, g2;                  // global var: 3, 4

namespace Alpha {
    extern double shared_var;  // global var: 5 (первое объявление)
    static float hidden_var;   // static var: 2
} // namespace Alpha

namespace Beta {
    extern double shared_var;  // global var: 6 (другой namespace -> другая переменная)
    static float hidden_var;   // static var: 3
} // namespace Beta

namespace Alpha {
    double shared_var = 1.0;   // игнорируется, так как уже объявлена выше в Alpha
} // namespace Alpha

namespace {
    extern int anon_extern;    // global var: 7
    static int anon_static;    // static var: 4
} // namespace

namespace {
    int anon_extern;           // игнорируется, повторное объявление в том же анонимном namespace
} // namespace

extern long outer_linkage;     // global var: 8
long outer_linkage = 100L;     // игнорируется, было объявлено строкой выше

struct DataBox { 
public: 
    DataBox(float v = 0.0f) : value(v) {} // func param: 1
    float value;
};

template<typename Type>
class Container {
public:
    Container(Type init_val) : item(init_val) {} // func param: 2
    Type item;
};

template<typename T1, typename T2>
static T1 process(T1 arg1, T2 arg2) { // func params: 3, 4
    static T1 cache;           // static var: 5
    return cache;
}

static int calculate(int x, double y) { // func params: 5, 6
    int result = x + static_cast<int>(y); // local var: 1
    return result;
}

int main(int argc, char** argv) { // func params: 7, 8

    static Container<int> c_int(5);   // static var: 6
    Container<double> c_double(3.14); // local var: 2

    DataBox box;                      // local var: 3
    
    int ans = calculate(argc, global_val); // local var: 4
    
    constexpr int limit = 100;        // local var: 5
    static constexpr float pi = 3.14f;// static var: 7
    
    int dummy_counter = 0;            // local var: 6
    
    return 0;
}