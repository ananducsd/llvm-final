; ModuleID = 'constprop2_out.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define i32 @main() #0 {
"0":
  br i1 false, label %"1", label %"5"

"1":                                              ; preds = %"0"
  br label %"2"

"2":                                              ; preds = %"3", %"1"
  %x.0 = phi i32 [ 9, %"1" ], [ %add, %"3" ]
  %add = add nsw i32 %x.0, 1
  br label %"3"

"3":                                              ; preds = %"2"
  %cmp2 = icmp slt i32 %add, 10
  br i1 %cmp2, label %"2", label %"4"

"4":                                              ; preds = %"3"
  br label %"6"

"5":                                              ; preds = %"0"
  br label %"6"

"6":                                              ; preds = %"5", %"4"
  %w.0 = phi i32 [ 3, %"4" ], [ 3, %"5" ]
  %mul4 = mul nsw i32 %w.0, 2
  ret i32 0
}

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = metadata !{metadata !"clang version 3.4 (tags/RELEASE_34/final)"}
