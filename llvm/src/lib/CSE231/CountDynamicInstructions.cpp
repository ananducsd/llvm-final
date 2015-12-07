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
  
  struct CountDynamicInstructions : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    CountDynamicInstructions() : ModulePass(ID) {}
    Function *hook;
    

    virtual void print(raw_ostream &O,const Module*) const;
    
    virtual bool runOnModule(Module &M) {	
    
         
     Module::iterator F,F_last;
     
     //errs() << "Entering a module " << "\n"; //<< M.getName();
     
     
     std::vector<Type *> argT;
     argT.push_back(Type::getInt32Ty(getGlobalContext()));argT.push_back(Type::getInt32Ty(getGlobalContext()));		
     FunctionType *FT = FunctionType::get(Type::getVoidTy(getGlobalContext()), argT,(Type *)0);
     Constant* c = M.getOrInsertFunction("update_map", FT);	

     FunctionType *FT2 =  FunctionType::get(Type::getVoidTy(getGlobalContext()), (Type *)0);
     Constant* c2 = M.getOrInsertFunction("print_map",FT2);
     	
     //hookFunc = M.getOrInsertFunction("update_map", Type::getVoidTy(M.getContext()), (Type*)0);
               
     Function* hook = cast<Function>(c);
     Function *printF = cast<Function>(c2);
			
     for (F = M.begin(), F_last = M.end(); F != F_last; ++F) {
	
      //errs() << "Entering a function " << F->getName() << "\n";
      bool isMain = F->getName().compare("main") == 0;
      Function::iterator blk,blk_last;
      for (blk = F->begin(), blk_last = F->end(); blk != blk_last; ++blk) {
          		
	   //Print out the name of the basic block if it has one, and then the
	  // number of instructions that it contains
	    //errs() << "Basic block (name=" << blk->getName() << ") has "
	  	     //<< blk->size() << " instructions.\n";
	
	
	   
 	std::map <int, int> count_map;
    	std::map <int, int>::iterator it;
	// blk is a pointer to a BasicBlock instance
	BasicBlock::iterator i,e,prev;
	

	for (i = blk->begin(), e = blk->end(); i != e; ++i) {
	   
	   prev = i;	
	   int instr_opcode = i->getOpcode();
	   std::string instr_name = i->getOpcodeName();		
	  

	
	    it = count_map.find(instr_opcode); 
            if (it != count_map.end()) {
              it->second = it->second + 1;
	    } else {
	      count_map.insert(std::make_pair(instr_opcode, 1));	
	    }	
          
	 }
         
	  // call the instrumentation module function with instruction name and count
	  //errs() << "Inserting instrumentation code for counting:\n";	
	  for(it = count_map.begin(); it != count_map.end(); ++it) {
		//Instruction *newInst = CallInst::Create(hook, it->first, it->second);
		//errs() << it->first << " " << it->second << "\n";		
		
		std::vector<Value *> ArgsV;

		ArgsV.push_back((Value *)builder.getInt32(it->first));
		ArgsV.push_back((Value *)builder.getInt32(it->second));
		
		//(Value *)IRBuilderBase::getInt32(it->first);
		//llvm::makeArrayRef (const ArrayRef< T > &Vec)

		//ArrayRef<Value *> argRef(ArgsV);
		//errs() << "1\n";		
		builder.SetInsertPoint(prev);
		//errs() << "2\n";		
		builder.CreateCall(hook, ArgsV);
		//errs() << "3\n";                	
                //blk->getInstList().insert(newInst); 
		//errs() << it->first << "\t" << it->second << "\n";
			
	   }  
	   //errs() << "Insertion done.\n";
	   
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

void CountDynamicInstructions::print(raw_ostream &O,const Module*) const{
	 	O << "Count dynamic pass done!! \n";	       
}

}


char CountDynamicInstructions::ID = 0;
static RegisterPass<CountDynamicInstructions> X("countDynamic", "count instructions: dynamic");
