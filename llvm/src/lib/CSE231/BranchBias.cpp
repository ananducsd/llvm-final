#include "llvm/Pass.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"

#include "llvm/IR/IRBuilder.h"

#include <iostream>
using namespace std;

using namespace llvm;

namespace {
    struct BranchBias : public ModulePass {
        static char ID;

        // Pointers to the helper functions
        Function *bAll, *bTrue, *printFn, *currFn;
        BasicBlock *lastBlock;

        //global ref to function name
        StringRef functionName;

        BranchBias() : ModulePass (ID) {}

        virtual bool runOnModule (Module &M) {
           
            bTrue = cast<Function> (M.getOrInsertFunction("branchTrue", 
                                          Type::getVoidTy(M.getContext()), 
                                          Type::getInt8PtrTy(M.getContext()), 
                                          (Type*)0));
            bAll = cast<Function> (M.getOrInsertFunction("branchAll", 
                                          Type::getVoidTy(M.getContext()), 
                                          Type::getInt8PtrTy(M.getContext()), 
                                          (Type*)0));
            printFn = cast<Function> (M.getOrInsertFunction("printResults", 
                                            Type::getVoidTy(M.getContext()), 
                                            (Type*)0));

            
            bTrue->setCallingConv(CallingConv::C);
            bAll->setCallingConv(CallingConv::C);

            for(Module::iterator F = M.begin(), E = M.end(); F!= E; ++F)
            {
                
                functionName = (*F).getName();
                currFn = F;
                if (F->getName() == "main") lastBlock = &(F->back());
                
                for(Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
                {
                    BranchBias::runOnBasicBlock(BB);
                }
            }
            return false;
        }

        virtual bool runOnBasicBlock (Function::iterator &BB) {
            
            Value *valPtr;
            bool createConst = true;

            for(BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI)
            {
                
                if(isa<BranchInst>(&(*BI)) ) {
                    BranchInst *CI = dyn_cast<BranchInst>(BI);
                    
                    if (CI->isUnconditional()) {
                        continue;
                    }

                    BasicBlock *takenBlock = CI->getSuccessor(0); 
                    BasicBlock *newBlock = BasicBlock::Create(getGlobalContext(), "newBlock", currFn);
                    IRBuilder<> builder(newBlock);
                    
                    if (createConst) {
                        valPtr = builder.CreateGlobalStringPtr(functionName, "valPtr");
                        createConst = false;
                    }
                    builder.CreateCall (bTrue, valPtr);
                    // Add unconditional jump to "taken" block
                    builder.CreateBr (takenBlock);

                    
                    CI->setSuccessor(0, newBlock); 
                    Instruction *takenIns = takenBlock->begin();
                    if (isa<PHINode>(takenIns)) {
                        PHINode *phi = cast<PHINode>(takenIns);
                        int bb_index = phi->getBasicBlockIndex(BB);
                        phi->setIncomingBlock (bb_index, newBlock);
                    }

                    builder.SetInsertPoint(CI);
                    builder.CreateCall (bAll, valPtr);

                    
                }
                
                // Add a print if it's a ret statement in main
                if ((isa<ReturnInst>(BI)) && (currFn->getName() == "main")) {
                    
                    IRBuilder<> builder2(BB);
                    builder2.SetInsertPoint(BI);
			        builder2.CreateCall(printFn, "");
                }
            }
            return true;
        }
       
    };
}

char BranchBias::ID = 0;
static RegisterPass<BranchBias> X("TestBranchBias", "Test Branch Bias per function", false, false);
