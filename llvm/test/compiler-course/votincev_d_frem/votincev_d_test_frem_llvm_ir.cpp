#include <cmath> // for frem


// frem == fp rem
// srem == signed rem
// urem == unsigned rem

int test_frem(double a,double b) {
  return std::fmod(a,b);
}

int test_srem(int a,int b) {
  return a % 2;
}

int test_urem(unsigned int a,unsigned int b) {
  return a % b;
}





// векторные типы через атрибуты Clang
typedef double double2 __attribute__((vector_size(16)));
typedef int int4 __attribute__((vector_size(16)));
typedef unsigned int uint4 __attribute__((vector_size(16)));

// тест на векторный frem (написан вручную - так как не переводится во что нужно)
// entry:
//  %div = frem <2 x double> %a, %b
//  ret <2 x double> %div
// }


// тест на векторный srem
int4 test_srem_vec(int4 a, int4 b) {
    return a % b; // Clang умеет применять % к целочисленным векторам!
}

// тест на векторный urem
uint4 test_urem_vec(uint4 a, uint4 b) {
    return a % b; // Clang умеет применять % к беззнаковым векторам!
}