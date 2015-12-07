#define DEBUG_TYPE "COUNT_DYNAMIC"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <vector>
#include <string.h>
#include <utility>
#include <iomanip>

using namespace llvm;

namespace {

  static IRBuilder<> builder(getGlobalContext());	
  
  
  
  struct BranchBias : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    BranchBias() : ModulePass(ID) {}
    Function *hook;
    StringRef fnName;

    virtual void print(raw_ostream &O,const Module*) const;
    
    virtual bool runOnModule(Module &M) {	
    
         
     Module::iterator F,F_last;
     
     errs() << "Entering a module " << "\n"; //<< M.getName();
     
     
     std::vector<Type *> argT;
     argT.push_back(Type::getInt8PtrTy(getGlobalContext()));
     argT.push_back(Type::getInt1Ty(getGlobalContext()));		
     FunctionType *FT = FunctionType::get(Type::getVoidTy(getGlobalContext()), argT,(Type *)0);
     Constant* c = M.getOrInsertFunction("update_biasmap", FT);	

     FunctionType *FT2 =  FunctionType::get(Type::getVoidTy(getGlobalContext()), (Type *)0);
     Constant* c2 = M.getOrInsertFunction("print_biasmap",FT2);
     	
     //hookFunc = M.getOrInsertFunction("update_map", Type::getVoidTy(M.getContext()), (Type*)0);
               
     Function* hook = cast<Function>(c);
     Function *printF = cast<Function>(c2);
			
     for (F = M.begin(), F_last = M.end(); F != F_last; ++F) {
	  
	  Value* myStr;
      errs() << "Entering a function " << F->getName() << "\n";
      bool isMain = F->getName().compare("main") == 0;
      Function::iterator blk,blk_last;
      for (blk = F->begin(), blk_last = F->end(); blk != blk_last; ++blk) {
          		
	   //Print out the name of the basic block if it has one, and then the
	  // number of instructions that it contains
	    errs() << "Basic block (name=" << blk->getName() << ") has "
	  	     << blk->size() << " instructions.\n";
	
	
	   
 	std::map <int, int> count_map;
    	std::map <int, int>::iterator it;
	// blk is a pointer to a BasicBlock instance
	BasicBlock::iterator i,e,prev;
	

	for (i = blk->begin(), e = blk->end(); i != e; ++i) {
	   
	   prev = i;	
	   int instr_opcode = i->getOpcode();
	   std::string instr_name = i->getOpcodeName();		
	   
       bool isBranch = isa<BranchInst>(i);
	    if(isBranch) {
	        BranchInst *CI = dyn_cast<BranchInst>(i);
            // We only care about conditional branches
            if (CI->isConditional()) {
                
                errs() << "\n1\n";
                Value* v = CI->getCondition();
                bool bval = (v != 0);
                
                errs() << "2\n";
                
                std::vector<Value *> ArgsV;
                fnName = F->getName();
                errs() << fnName << " " << *v << " 3\n";
                
                IRBuilder<> builder2(blk);
                errs() << "3.5\n";
                myStr = (Value *)builder2.CreateGlobalStringPtr(fnName);
                errs() << "4\n";
		        
		        ArgsV.push_back(myStr);
		        ArgsV.push_back((Value *)builder.getInt1(bval));
                
                errs() << "5\n";
                builder.SetInsertPoint(i);
			    builder.CreateCall(hook,ArgsV);	
                
                //CI->setCondition((Value *)builder.getInt1(bval));
                errs() << instr_name << "\n";
            }
        }
	 }
         
	   // Inserting code for printing results before return stmt in main
		for (i = blk->begin(), e = blk->end(); i != e; ++i) {
	   
	   	
	   
	   std::string instr_name = i->getOpcodeName();		
	  
	   bool isRetStmt = instr_name.compare("ret") == 0;
		   
		if(isRetStmt && isMain) {
			//errs() << "*********Inserting call to print Map before return in main*************\n";
			builder.SetInsertPoint(i);
			builder.CreateCall(printF);	
		}	
	
	   
          
	 }
	 				   	
       }
     }	

 	return true;
    }

	
  };

void BranchBias::print(raw_ostream &O,const Module*) const{
	 	O << "Branch bias pass done!! \n";	       
}

}


char BranchBias::ID = 0;
static RegisterPass<BranchBias> X("branchBias", "Branch Bias");
