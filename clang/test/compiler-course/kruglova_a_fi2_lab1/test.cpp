// RUN: %clang_cc1 -load %llvmshlibdir/kruglova_a_fi2_lab1_ClangAST%pluginext -plugin kruglova_plugin -fsyntax-only %s 2>&1 | FileCheck %s

int g_1;

namespace Data {
  int g_namespace;
}

 static int s_1;  //глобальная static - static

namespace Extra {
  static int ns_static = 10;
}
struct MyStruct {
  static int s_2;
};

void test_func(int p1, char p2) {

  int l_1 = 10;

  static int s_3 = 5;

  for (int i = 0; i < 10; ++i) {     // i в for - locals
    int l_2 = i;
  }

  if (int status = p1; status > 0) {    // в if-init - local
    int l_inner = 1;
  }
}


void loop_case() {
  int x = 0;

  while (int w = x++) {    // w объявлена в while - local
    int inner = w;
  }
}

auto lambda = [](int p_lambda) {
  int l_lambda = 100;                // локальная внутри лямбды
};

void lambda_test() {

  auto inner_lambda = [](double p_l) {
    int l_inside = 0;
  };

}

void switch_case(int p) {

  switch (p) {

    case 1: {
      int local_switch = 5;   // local в switch
      break;
    }

    default:
      break;
  }
}

void block_scope() {
  {
    int deep_local = 1;
  }
}


// Globals: 3 (g_1, g_namespace, lambda)
// Statics: 4 (s_1, ns_static, s_2, s_3)
// Locals:  13 (l_1, i, l_2, status, l_inner, x, w, inner, l_lambda, inner_lambda, l_inside, local_switch, deep_local)
// Params:  5 (p1, p2, p_lambda, p_l, p)
// Total:   25

// CHECK: Statistics
// CHECK-NEXT: Globals variables: 3
// CHECK-NEXT: Statics variables: 4
// CHECK-NEXT: Locals variables:  13   
// CHECK-NEXT: Params variables:  5
// CHECK-NEXT: Total variables:   25   