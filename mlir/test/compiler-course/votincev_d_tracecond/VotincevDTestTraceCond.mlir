// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/votincev_d_trace_cond_MLIR%shlibext \
// RUN:   --pass-pipeline="builtin.module(votincev_d_trace_cond_MLIR)" %s | FileCheck %s

// »спользуем CHECK-DAG, так как пор€док объ€влений в начале модул€ зависит от логики вставки (LIFO)
// CHECK-DAG: func.func private @trace_condition_then_begin()
// CHECK-DAG: func.func private @trace_condition_then_end()
// CHECK-DAG: func.func private @trace_condition_else_begin()
// CHECK-DAG: func.func private @trace_condition_else_end()

// CHECK-LABEL: func.func @test_scf_if
func.func @test_scf_if(%cond: i1) {
  // CHECK: scf.if
  scf.if %cond {
    // CHECK-NEXT: func.call @trace_condition_then_begin()
    %c1 = arith.constant 1 : i32
    // CHECK: func.call @trace_condition_then_end()
    // CHECK-NEXT: } else {
  } else {
    // CHECK-NEXT: func.call @trace_condition_else_begin()
    %c2 = arith.constant 2 : i32
    // CHECK: func.call @trace_condition_else_end()
    // CHECK-NEXT: }
  }
  return
}

// CHECK-LABEL: func.func @test_affine_if
func.func @test_affine_if(%arg0: index) {
  // CHECK: affine.if
  affine.if affine_set<(d0) : (d0 - 1 >= 0)>(%arg0) {
    // CHECK-NEXT: func.call @trace_condition_then_begin()
    %c1 = arith.constant 1 : i32
    // CHECK: func.call @trace_condition_then_end()
    // CHECK-NEXT: } else {
  } else {
    // CHECK-NEXT: func.call @trace_condition_else_begin()
    %c2 = arith.constant 2 : i32
    // CHECK: func.call @trace_condition_else_end()
    // CHECK-NEXT: }
  }
  return
}