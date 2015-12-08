#source $CSE231ROOT/startenv.sh
CPPFLAGS=`llvm-config --cppflags`
LLVMLIBS=`llvm-config --libs`
LDFLAGS=`llvm-config --ldflags`


function runConstProp {
# compile to bitcode
clang -emit-llvm -c $1.c -o temp.bc
#run mem to reg pass - use registers as much as possible
opt -mem2reg < temp.bc > $1_in.bc
#run our constant propagation pass
opt -load $LLVMLIB/CSE231.so -ConstantProp < $1_in.bc > $1_out.bc
#disassemble the bitcodes for debugging
llvm-dis $1_in.bc

llvm-dis $1_out.bc

rm temp.bc

}


#!/bin/bash
FILES=$CSE231ROOT/temp/*.c


cd $CSE231ROOT/temp
make -C ..
rm *.bc
rm *.ll


for f in $FILES
do
  filename=$(basename "$f")
  extension="${filename##*.}"
  filename="${filename%.*}"
  runConstProp $filename
done



