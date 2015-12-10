
# passes available

# CSEPass           : common subexpression elimination
# RangePass         : Range analysis
# BranchFoldingPass : Branch folding
# ConstPass 		: Constant propagation and folding
# alias_analysis		: Pointer Analysis
if [ $# -ne 2 ]; then
        echo "usage: ./compileAndRunAnalysis.sh file-name pass-name"
        exit 1
fi

fname=${1%.*}
passname=${2%.*}

echo "Setting Up environment . . . "
if test -e $fname.bc ; then
    rm $fname.bc
fi

if test -e ${fname}_in.bc; then
    rm ${fname}_in.bc
fi

if test -e  ${fname}_out.bc; then
    rm  ${fname}_out.bc
fi

if test -e  ${fname}_in.ll; then
    rm  ${fname}_in.ll
fi

if test -e  ${fname}_out.ll ; then
    rm  ${fname}_out.ll
fi


echo "Compiling file $1 and using $fname . . . "

clang -emit-llvm -c $1 -o ${fname}_in.bc
echo "Generating $fname.bc and running mem2reg pass . . . "
#opt -mem2reg < $fname.bc > ${fname}_in.bc
echo "Running ${passname} on ${fname}_in.bc . . . "
echo -e "\n-----------------------------------------------------------------------"
#opt -load $LLVMLIB/SidGroup.so -${passname} < ${fname}_in.bc > ${fname}_out.bc
opt -load $LLVMLIB/CSE231.so -${passname} < ${fname}_in.bc > ${fname}_out.bc
echo -e "\n-----------------------------------------------------------------------"
echo "Converting generated BitCode files into human readable format . . . "
llvm-dis ${fname}_in.bc
llvm-dis ${fname}_out.bc
echo -e "Success\n Unoptimized output: ${fname}_in.ll\n Optimized output: ${fname}_out.ll"
