
# passes available

# CSEPass           : common subexpression elimination
# RangePass         : Range analysis
# BranchFoldingPass : Branch folding
# ConstPass 		: Constant propagation and folding

if [ $# -ne 2 ]; then
        echo "usage: ./compileAndRunAnalysis.sh file-name pass-name"
        exit 1
fi

fname=${1%.*}
passname=${2%.*}

echo "Setting Up environment . . . "
rm $fname.bc ${fname}_in.bc ${fname}_out.bc
rm ${fname}_in.ll ${fname}_out.ll
echo "Compiling file $1 and using $fname . . . "

clang -emit-llvm -c $1 -o $fname.bc
echo "Generating $fname.bc and running mem2reg pass . . . "
opt -mem2reg < $fname.bc > ${fname}_in.bc
echo "Running Constant Propagation on ${fname}_reg.bc . . . "
echo -e "\n-----------------------------------------------------------------------"
opt -load $LLVMLIB/SidGroup.so -${passname} < ${fname}_in.bc > ${fname}_out.bc
echo -e "\n-----------------------------------------------------------------------"
echo "Converting generated BitCode files into human readable format . . . "
llvm-dis ${fname}_in.bc
llvm-dis ${fname}_out.bc
echo -e "Success\n Unoptimized output: ${fname}_in.ll\n Optimized output: ${fname}_out.ll"
