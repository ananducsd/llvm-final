if [ $# -ne 1 ]; then
        echo "usage: ./compileAndRunAnalysis.sh file-name"
        exit 1
fi

fname=${1%.*}
echo "Compiling file $1 and using $fname..."

clang -emit-llvm -c $1 -o $fname.bc
echo "Generating $fname.bc and running mem2reg pass..."
opt -mem2reg < $fname.bc > ${fname}_reg.bc
echo "Running Constant Propagation on ${fname}_reg.bc..."
echo "-----------------------------------------------------------------------"
opt -load $LLVMLIB/CSE231.so -ConstantProp < ${fname}_reg.bc > ${fname}_instr.bc
echo "-----------------------------------------------------------------------"
echo "Converted generated BitCode files into human readable format"
llvm-dis ${fname}_reg.bc
llvm-dis ${fname}_instr.bc
echo -e "Success\n Unoptimized output: ${fname}_reg.bc\n Optimized output: ${fname}_instr.bc"
