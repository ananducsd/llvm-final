#define DEBUG_TYPE "COUNT_STATIC"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <string.h>
#include <utility>

using namespace llvm;

namespace {
  
  struct CountStaticInstructions : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    CountStaticInstructions() : ModulePass(ID) {}

    mutable std::map <std::string, int> count_map;
    mutable std::map <std::string, int>::iterator it;

    virtual void print(raw_ostream &O,const Module*) const;
    
    virtual bool runOnModule(Module &M) {	
    
     
     
     Module::iterator F,F_last;
     //errs() << "Entering a module " << "\n"; //<< M.getName();

     for (F = M.begin(), F_last = M.end(); F != F_last; ++F) {
	
      //errs() << "Entering a function " << F->getName() << "\n";
      Function::iterator blk,blk_last;
      for (blk = F->begin(), blk_last = F->end(); blk != blk_last; ++blk) {
          		
	  // Print out the name of the basic block if it has one, and then the
	  // number of instructions that it contains
	  //errs() << "Basic block (name=" << blk->getName() << ") has "
	  //	     << blk->size() << " instructions.\n";
	
	 
	// blk is a pointer to a BasicBlock instance
	BasicBlock::iterator i,e;
	for (i = blk->begin(), e = blk->end(); i != e; ++i) {
	   
	   std::string instr_name = i->getOpcodeName();
	   //errs() << instr_name << "\n";
	 
	    it = count_map.find(instr_name); 
            if (it != count_map.end()) {
              it->second = it->second + 1;
	    } else {
	      count_map.insert(std::make_pair(instr_name, 1));	
	    }	
          
	 } 
       }
     }	

 	return false;
    }

	
  };

void CountStaticInstructions::print(raw_ostream &O,const Module*) const{
	    
	   // print the total counts
	 	//O << "Helloss!! \n";
           
	       int total = 0;

	       for(it = count_map.begin(); it != count_map.end(); ++it) {
			O << it->first << "\t" << it->second << "\n";
			total += it->second;
		}
		O << "Total\t" << total << "\n";
}

}


char CountStaticInstructions::ID = 0;
static RegisterPass<CountStaticInstructions> X("countStatic", "count instructions: static");
