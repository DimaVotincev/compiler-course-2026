; RUN: opt -load-pass-plugin %llvmshlibdir/votincev_d_frem_LLVM_IR%pluginext \
; RUN: -passes=votincev_d_frem -S %s | FileCheck %s

; --- СКАЛЯРНЫЕ ТЕСТЫ ---

; CHECK-LABEL: define dso_local noundef i32 @_Z9test_fremdd(
; CHECK-SAME: double noundef %a, double noundef %b)
; CHECK-NOT: frem
; CHECK: [[DIV:%[0-9]+]] = fdiv double %a, %b
; CHECK: [[TRUNC:%[0-9]+]] = call double @llvm.trunc.f64(double [[DIV]])
; CHECK: [[MUL:%[0-9]+]] = fmul double [[TRUNC]], %b
; CHECK: [[SUB:%[0-9]+]] = fsub double %a, [[MUL]]
; CHECK: fptosi double [[SUB]] to i32
define dso_local noundef i32 @_Z9test_fremdd(double noundef %a, double noundef %b) local_unnamed_addr #0 {
entry:
  %fmod = frem double %a, %b
  %conv = fptosi double %fmod to i32
  ret i32 %conv
}

; CHECK-LABEL: define dso_local noundef range(i32 -1, 2) i32 @_Z9test_sremii(
; CHECK-NOT: srem
; CHECK: [[DIV:%[0-9]+]] = sdiv i32 %a, 2
; CHECK: [[MUL:%[0-9]+]] = mul i32 [[DIV]], 2
; CHECK: sub i32 %a, [[MUL]]
define dso_local noundef range(i32 -1, 2) i32 @_Z9test_sremii(i32 noundef %a, i32 noundef %b) local_unnamed_addr #0 {
entry:
  %rem = srem i32 %a, 2
  ret i32 %rem
}

; CHECK-LABEL: define dso_local noundef i32 @_Z9test_uremjj(
; CHECK-NOT: urem
; CHECK: [[DIV:%[0-9]+]] = udiv i32 %a, %b
; CHECK: [[MUL:%[0-9]+]] = mul i32 [[DIV]], %b
; CHECK: sub i32 %a, [[MUL]]
define dso_local noundef i32 @_Z9test_uremjj(i32 noundef %a, i32 noundef %b) local_unnamed_addr #0 {
entry:
  %rem = urem i32 %a, %b
  ret i32 %rem
}


; --- ВЕКТОРНЫЕ ТЕСТЫ ---

; CHECK-LABEL: define dso_local noundef <2 x double> @_Z13test_frem_vecDv2_dS_(
; CHECK-NOT: frem
; CHECK: [[DIV:%[0-9]+]] = fdiv <2 x double> %a, %b
; CHECK: [[TRUNC:%[0-9]+]] = call <2 x double> @llvm.trunc.v2f64(<2 x double> [[DIV]])
; CHECK: [[MUL:%[0-9]+]] = fmul <2 x double> [[TRUNC]], %b
; CHECK: fsub <2 x double> %a, [[MUL]]
define dso_local noundef <2 x double> @_Z13test_frem_vecDv2_dS_(<2 x double> noundef %a, <2 x double> noundef %b) local_unnamed_addr #1 {
entry:
  %div = frem <2 x double> %a, %b
  ret <2 x double> %div
}

; CHECK-LABEL: define dso_local noundef <4 x i32> @_Z13test_srem_vecDv4_iS_(
; CHECK-NOT: srem
; CHECK: [[DIV:%[0-9]+]] = sdiv <4 x i32> %a, %b
; CHECK: [[MUL:%[0-9]+]] = mul <4 x i32> [[DIV]], %b
; CHECK: sub <4 x i32> %a, [[MUL]]
define dso_local noundef <4 x i32> @_Z13test_srem_vecDv4_iS_(<4 x i32> noundef %a, <4 x i32> noundef %b) local_unnamed_addr #1 {
entry:
  %rem = srem <4 x i32> %a, %b
  ret <4 x i32> %rem
}

; CHECK-LABEL: define dso_local noundef <4 x i32> @_Z13test_urem_vecDv4_jS_(
; CHECK-NOT: urem
; CHECK: [[DIV:%[0-9]+]] = udiv <4 x i32> %a, %b
; CHECK: [[MUL:%[0-9]+]] = mul <4 x i32> [[DIV]], %b
; CHECK: sub <4 x i32> %a, [[MUL]]
define dso_local noundef <4 x i32> @_Z13test_urem_vecDv4_jS_(<4 x i32> noundef %a, <4 x i32> noundef %b) local_unnamed_addr #1 {
entry:
  %rem = urem <4 x i32> %a, %b
  ret <4 x i32> %rem
}
