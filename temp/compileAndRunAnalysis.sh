if [ $# -ne 1 ]; then
        echo "usage: ./compileAndRunAnalysis.sh file-name"
        exit 1
fi

fname=${1%.*}
echo "Compiling file $1 and using $fname"

clang -emit-llvm -c $1 -o $fname.bc
opt -mem2reg < $fname.bc > ${fname}_reg.bc
opt -load $LLVMLIB/CSE231.so -ConstantProp < ${fname}_reg.bc > ${fname}_instr.bc
llvm-dis ${fname}_reg.bc
llvm-dis ${fname}_instr.bc

