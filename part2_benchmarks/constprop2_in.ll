; ModuleID = 'constprop2_in.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define i32 @main() #0 {
entry:
  %cmp = icmp slt i32 9, 3
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  br label %do.body

do.body:                                          ; preds = %do.cond, %if.then
  %x.0 = phi i32 [ 9, %if.then ], [ %add, %do.cond ]
  %add = add nsw i32 %x.0, 1
  %add1 = add nsw i32 2, 1
  br label %do.cond

do.cond:                                          ; preds = %do.body
  %cmp2 = icmp slt i32 %add, 10
  br i1 %cmp2, label %do.body, label %do.end

do.end:                                           ; preds = %do.cond
  br label %if.end

if.else:                                          ; preds = %entry
  %mul = mul nsw i32 9, 2
  %add3 = add nsw i32 %mul, 5
  br label %if.end

if.end:                                           ; preds = %if.else, %do.end
  %w.0 = phi i32 [ %add1, %do.end ], [ 3, %if.else ]
  %mul4 = mul nsw i32 %w.0, 2
  ret i32 0
}

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = metadata !{metadata !"clang version 3.4 (tags/RELEASE_34/final)"}
