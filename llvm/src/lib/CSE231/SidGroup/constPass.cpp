#define DEBUG_TYPE "ConstPass"
#include <map>
#include "constObjects.cpp"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

//opt -load $LLVMLIB/CSE231.so -ConstPass < $BENCHMARKS/gcd/gcd.bc > temp.instrumented.bc

using namespace llvm;
namespace {
	static IRBuilder<> builder(getGlobalContext());
	
  struct ConstPass : public ModulePass {
    static char ID; // Pass identification, replacement for typeid

    //iterate through all instructions, fill map.
    ConstPass() : ModulePass(ID) {
			//errs() << "Const Module Created" << "\n";
		}
    
    bool runOnModule(Module &M){
      
      //create the worklist object
      const_workListObj* wl = new const_workListObj(10010);
      wl->init(M);
      wl->run();

      //M.dump();
      //wl->printTop();
      //wl->printBottom();
      
      //For each BBNode
      for(map<int, const_BBNode*>::iterator it = wl->bbMap.begin(); it != wl->bbMap.end(); it++) {
        const_BBNode * BBN = it->second;
        //For each node
        for(unsigned int i=0; i<BBN->nodes.size(); i++){
          const_Node * N = BBN->nodes[i];
          //if not an alloc instruction,
          if(N->oI->getOpcode() != Instruction::Alloca){
            //if there exists an entry for this instruction
            if(N->e->data->count(N->oI) == 1 ){
              const_Fact * c = N->e->data->find(N->oI)->second;
              if(c->possibleValues.size() == 1 && !c->isBottom){
                set<Value *>::iterator itl = c->possibleValues.begin();
                Value * result = *itl;
                BasicBlock::iterator ii(N->oI);
                ReplaceInstWithValue(N->oI->getParent()->getInstList(), ii, result);
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

char ConstPass::ID = 0;
static RegisterPass<ConstPass> X("ConstPass", "Const Pass",false,false);
