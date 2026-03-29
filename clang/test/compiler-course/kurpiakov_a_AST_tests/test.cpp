// RUN: %clang_cc1 -load %llvmshlibdir/VarStatPlugin_Kurpiakov_Aleksei_FIIT3_ClangAST%pluginext -plugin var_statistic -fsyntax-only %s 2>&1 | FileCheck %s

//CHECK: Total count : 27
//CHECK-NEXT: Global variables : 9
//CHECK-NEXT: Static variables : 8
//CHECK-NEXT: Local variables  : 5
//CHECK-NEXT: Function params  : 5

static int x = 0; //  static var count = 0 + 1
long y = 0L; //  global var count = 0 + 1


namespace N {
    extern int n; // +1 к global var count 
    static int f; // +1 к static var count
}

namespace Z {
    extern int n; // +1 к global var count тк разные namespace
    static int f; // +1 к static var count тк разные namespace
}

namespace {
    extern int n; // +1 к global var count тк разные namespace
    static int f; // +1 к static var count тк разные namespace

}

namespace {
    int n; // не посчитается, тк переменная была объявлена выше в том же (анонимном) namespace
}


namespace X {
    int n; // +1 к global var count тк разные namespace
    extern int n; // не посчитается, тк переменная была объявлена выше в том же namespace
}

extern int g; // +1 к global var count 
int g; // не посчитается, тк переменная была объявлена выше

template<typename Y> // все что связанно с полями класса не входит в VarDecl, а является FieldDecl
class Class{
public:
    Class(): a(0), k(0) {}
    int a;
    Y k;
};

struct Struct{ // все что связанно с полями класса/структуры не входит в VarDecl, а является FieldDecl
public:
    Struct(int num = 0): a(0), k(0) {} // func parm count = 0 + 1
    int a;
    double k;
};

template<typename T> 
T foo(T a, T b){ // func parm count = 1 + 2
    static int z = 52; // static var count = 4 + 1
    return z;
}


template<typename T = char>
char foo(int a, int b){ //func parm count = 3 + 2
    int z = 42 + static_cast<char>(x) + static_cast<char>(y); // local var count = 0 + 1
    return z;
}

int a, b, c; // global var count = 6 + 3

int main(){

    static Class<int> c_int; // static var count = 5 + 1
    Class<float> c_float; // local var count = 1 + 1

    Struct st; // local var count = 2 + 1
    
    char z = foo(a, b); // local var count = 3 + 1
    
    static constexpr int var = 52; // static var count = 6 + 1
    constexpr float var_f = 0.0f;  // local var count = 4 + 1
    
    static int var2  = 52; // static var count = 7 + 1
    
    return 0;
}

/*
total count = 27
global var count = 9
static var count = 8
local var count = 5
func parm count = 5
*/ 
