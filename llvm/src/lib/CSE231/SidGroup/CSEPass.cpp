#define DEBUG_TYPE "ConstPass"
#include <map>
#include "CSEObjects.cpp"

/*
*/

//opt -load $LLVMLIB/CSE231.so -CSEPass < $BENCHMARKS/gcd/gcd.bc > temp.instrumented.bc

using namespace llvm;
namespace {
	static IRBuilder<> builder(getGlobalContext());
	
  struct CSEPass : public ModulePass {
    static char ID; // Pass identification, replacement for typeid

    //iterate through all instructions, fill map.
    CSEPass() : ModulePass(ID) {
			//errs() << "CSE Module Created" << "\n";
		}

    bool runOnModule(Module &M){
      
      //create the worklist object
      CSE_workListObj* wl = new CSE_workListObj(10010);
      wl->init(M);
      wl->run();
      //print results...
      //M.dump();
      //wl->printTop();
      //wl->printBottom();   
      
      bool isChanged = true;
      while(isChanged == true){
        isChanged = false;
        for(map<int, CSE_BBNode*>::iterator it = wl->bbMap.begin(); it != wl->bbMap.end(); it++) {
          CSE_BBNode * BBN = it->second;
          //For each node
          for(unsigned int i=0; i<BBN->nodes.size(); i++){
          //aExpr
            CSE_Node * N = BBN->nodes[i];
            //update the current available expressions (remove any duplicates)
            for(set<Instruction*>::iterator it = N->e->aExpr.begin(); it != N->e->aExpr.end(); it++) {
              Instruction * compare = *it;
              //general case:
              if(compare->isIdenticalTo(N->oI) && N->oI->getOpcode() != Instruction::Alloca){
                //replace all instructions with the available expression!
                N->oI->replaceAllUsesWith(compare);
                isChanged = false;
              }
            }   
          }
        }
      }
      M.dump();
      return false;
    }
  };
}

char CSEPass::ID = 0;
static RegisterPass<CSEPass> X("CSEPass", "CSE Pass",false,false);
