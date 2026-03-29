// RUN: %clang_cc1 -load %llvmshlibdir/zavyalov_a_lab1_ClangAST%pluginext -plugin zavyalov_a_lab1_plugin -fsyntax-only %s 2>&1 | FileCheck %s

class A {
    int field = 0;
public:
    void nonconstMethod() {
        ++field;
    }

    int constMethod() const {
        return 44 + 5;
    }
};

void foo_nonconst(int& x) {
    x += 5;
}

void foo_const_ref(const int& x) {
}

void foo_value(int x) {
    x++;
}

void foo_address(int* x) {
    *x = 42;
}

void example() {
    // Pointers that can be made const
    
    int* x = new int(10);
    int* z = new int(10);
    int ** px = &x; 
    int *** ppx = &px; 
    int **** pppx = &ppx;
    int* sum = new int(*x + *z); 

    // CHECK: const int * const x = new int(10);
    // CHECK-NEXT: const int * const z = new int(10); 
    // CHECK-NEXT: const int ** const px = &x;
    // CHECK-NEXT: const int *** const ppx = &px;
    // CHECK-NEXT: const int **** const pppx = &ppx;
    // CHECK-NEXT: const int * const sum = new int(*x + *z);

    A *custom_class_test_const = new A();
    custom_class_test_const->constMethod();
    // CHECK: const A * const custom_class_test_const = new A();

    // Pointers that can not be made const

    int* y = new int(10);
    // CHECK-NOT: const int * const y
    // CHECK: int* y = new int(10);
    
    // second_y is changed with y
    int* second_y = y;
    // CHECK-NOT: const int * const second_y
    // CHECK: int* second_y = y;
    
    *y = 25;
    
    // Изменяется через указатель
    int* w = new int(10);
    // CHECK-NOT: const int * const w
    // CHECK: int* w = new int(10);

    int** w_ptr = &w;
    // CHECK-NOT: const int ** const w_ptr
    // CHECK: int** w_ptr = &w;
    
    *w_ptr = new int(45);

    // third_y is the same as second_y but declared after y's change
    int* third_y = y;
    // CHECK-NOT: const int * const third_y
    // CHECK: int* third_y = y;

    int* ptr_reassigned = new int(40);
    // CHECK-NOT: const int * const ptr_reassigned
    // CHECK: int* ptr_reassigned = new int(40);
    
    ptr_reassigned = new int(30);

    int* ptr_incr = new int(10);
    // CHECK-NOT: const int * const ptr_incr
    // CHECK: int* ptr_incr = new int(10);
    
    ptr_incr++;
    
    A *custom_class_test_nonconst = new A();
    // CHECK-NOT: const A * const custom_class_test_nonconst
    // CHECK: A *custom_class_test_nonconst = new A();
    
    custom_class_test_nonconst->nonconstMethod();


    // References that can not be made const
    int t = 5;
    
    int &t_ref_nonconst_assgn = t;
    // CHECK-NOT: const int &t_ref_nonconst_assgn
    // CHECK: int &t_ref_nonconst_assgn = t;
    
    t_ref_nonconst_assgn = 6;

    int &t_ref_nonconst_compound_add = t;
    // CHECK-NOT: const int &t_ref_nonconst_compound_add
    // CHECK: int &t_ref_nonconst_compound_add = t;
    
    t_ref_nonconst_compound_add += 4;

    int &t_ref_nonconst_incr = t;
    // CHECK-NOT: const int &t_ref_nonconst_incr
    // CHECK: int &t_ref_nonconst_incr = t;
    
    t_ref_nonconst_incr++;

    int &t_ref_nonconst_func_argument = t;
    // CHECK-NOT: const int &t_ref_nonconst_func_argument
    // CHECK: int &t_ref_nonconst_func_argument = t;
    
    foo_nonconst(t_ref_nonconst_func_argument);

    int &t_ref_nonconst_func_argument_by_address = t;
    // CHECK-NOT: const int &t_ref_nonconst_func_argument_by_address
    // CHECK: int &t_ref_nonconst_func_argument_by_address = t;
    
    foo_address(&t_ref_nonconst_func_argument_by_address);

    int &t_ref_nonconst_initializer = t;
    // CHECK-NOT: const int &t_ref_nonconst_initializer
    // CHECK: int &t_ref_nonconst_initializer = t;
    
    int &t_ref_nonconst_initialized_by_another_ref = t_ref_nonconst_initializer;
    // CHECK-NOT: const int &t_ref_nonconst_initialized_by_another_ref
    // CHECK: int &t_ref_nonconst_initialized_by_another_ref = t_ref_nonconst_initializer;
    
    t_ref_nonconst_initializer += 5;

    A a;
    A &a_nonconst_ref = a;
    // CHECK-NOT: const A &a_nonconst_ref
    // CHECK: A &a_nonconst_ref = a;
    
    a_nonconst_ref.nonconstMethod();


    // References that can be made const
   
    int &t_ref_const_unchanged = t;
    // CHECK: const int &t_ref_const_unchanged = t;

    int &t_ref_const_func_argument_ref = t;
    foo_const_ref(t_ref_const_func_argument_ref);
    // CHECK: const int &t_ref_const_func_argument_ref = t;

    int &t_ref_const_func_argument_value = t; 
    foo_value(t_ref_const_func_argument_value);
    // CHECK: const int &t_ref_const_func_argument_value = t;

    A &a_const_ref = a; 
    a_const_ref.constMethod();
    // CHECK: const A &a_const_ref = a;

    ++t;

    // Already const references and pointers
    const int &t_r = t;
    // CHECK: const int &t_r = t;
    
    const int* const ptr_const = new int(5); 
    // CHECK: const int* const ptr_const = new int(5);
}