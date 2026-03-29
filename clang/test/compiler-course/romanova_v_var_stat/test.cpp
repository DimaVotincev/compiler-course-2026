// RUN: %clang_cc1 -load %llvmshlibdir/romanova_v_var_stat_ClangAST%pluginext -plugin romanova_v_var_stat_plugin -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK: static variables: 14
// CHECK-NEXT: local variables: 11
// CHECK-NEXT: global variables: 13
// CHECK-NEXT: function parameters: 6
// CHECK-NEXT: total: 44

static int static_global_1 = 0;           // +1 static
static int static_global_2 = 1;           // +1 static

long global_long_1 = 0L;                  // +1 global
int global_int_1 = 42;                    // +1 global

extern int extern_global_1;               // не считается, тк просто extern
extern int extern_global_2;               // не считается, тк просто extern

extern int global_int_2;                  // не считается, тк просто extern
int global_int_2;                         // +1 global           

const int const_global = 100;             // +1 global


namespace N {
    extern int nz_extern;                 // не считается, тк просто extern
    static int n_static;                  // +1 static
    int nz_plain;                          // +1 global
    extern int nz_plain;                   // не считается, тк просто extern
}

namespace Z {
    extern int nz_extern;                  // не считается, тк просто extern
    static int nz_static;                  // +1 static (другой namespace)
    int nz_plain;                          // +1 global (другой namespace)
}

namespace { // Анонимный namespace
    extern int anon_1_extern;              // не считается, тк просто extern
    static int anon_1_static;              // +1 static
}

namespace { // Анонимный namespace 
    extern int anon_2_extern;              // не считается, тк просто extern
    static int anon_2_static;              // +1 static
    int anon_1_extern = 4;                 // +1 global
}

namespace X {
    int x_plain;                           // +1 global
    extern int x_plain;                    // не считается, тк просто extern
    int x_another;                         // +1 global
}


template<typename T>
class TemplateClass {
public:
    TemplateClass() : member(0), t_member() {}
    
    int member;                                 // FieldDecl - НЕ считаем
    T t_member;                                 // FieldDecl - НЕ считаем
    static int static_member;                   // +1 static
    
    void method(T param) {                      // +1 func param
        int local_in_method = 42;               // +1 local
        static int static_local_in_method = 0;  // +1 static
    }
};

struct SimpleStruct {
    int a;                                      // FieldDecl - НЕ считаем
    double b;                                   // FieldDecl - НЕ считаем
    static int struct_static;                   // +1 static
};

int bar(int x, int y, double z) {               // +3 func param
    int local_bar = 10;                         // +1 local
    static int static_bar = 0;                  // +1 static
    return local_bar + static_bar;
}


int a, b, c;                                    // +3 global

int d = 10;                                     // +1 global
extern int d;                                   // не считается, тк просто extern



int main(int argc, char* argv[]) {                 // +2 func param
    
    int local_1 = 0;                               // +1 local
    double local_2 = 3.14;                         // +1 local 
    char local_3 = 'C';                            // +1 local 
    
    static int static_local_1 = 100;               // +1 static 
    static float static_local_2 = 2.5f;            // +1 static 
    
    TemplateClass<int> tpl_obj;                    // +1 local 
    static TemplateClass<double> static_tpl_obj;   // +1 static 
    
    SimpleStruct s;                                // +1 local 
    
    int result = bar(1, 2, 3.0);                   // +1 local 
    
    {
        int block_local = 300;                     // +1 local 
        static int block_static = 400;             // +1 static 
        
        for (int i = 0; i < 10; ++i) {             // +1 local 
            int loop_local = i * 2;                // +1 local 
        }
    }
    
    return 0;
}