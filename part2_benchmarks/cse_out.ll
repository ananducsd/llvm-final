; ModuleID = 'cse_out.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define i32 @f(i32 %a, i32 %b) #0 {
"0":
  %add = add nsw i32 %a, %b
  %add1 = add nsw i32 %a, %b
  %add2 = add nsw i32 %add, %add
  ret i32 %add2
}

; Function Attrs: nounwind uwtable
define i32 @main() #0 {
"1":
  %call = call i32 @f(i32 2, i32 3)
  ret i32 0
}

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = metadata !{metadata !"clang version 3.4 (tags/RELEASE_34/final)"}
