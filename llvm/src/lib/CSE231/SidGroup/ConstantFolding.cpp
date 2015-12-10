#define DEBUG_TYPE "ConstPass"
#include <map>
#include "ConstantPropagation.cpp"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"


using namespace llvm;
using namespace constprop;
using namespace metalattice;

namespace {
	static IRBuilder<> builder(getGlobalContext());

  struct ConstantFoldingPass : public ModulePass {
    static char ID; // Pass identification, replacement for typeid

    //iterate through all instructions, fill map.
    ConstantFoldingPass() : ModulePass(ID) {
			//errs() << "Const Module Created" << "\n";
		}

    bool runOnModule(Module &M){

      //create the worklist object
      Worklist<EdgeFact, FlowFunctions, Lattice>* wl = new Worklist<EdgeFact, FlowFunctions, Lattice>(10010);
      wl->init(M);
      wl->run();

      //M.dump();
      //wl->printTop();
      //wl->printBottom();

      //For each BBNode<EdgeFact>
      for(map<int, BBNode<EdgeFact>*>::iterator it = wl->bbMap.begin(); it != wl->bbMap.end(); it++) {
        BBNode<EdgeFact> * BBN = it->second;
        //For each node
        for(unsigned int i=0; i<BBN->nodes.size(); i++){
          Node<EdgeFact> * N = BBN->nodes[i];
          //if not an alloc instruction,
          if(N->I->getOpcode() != Instruction::Alloca){
            //if there exists an entry for this instruction
            if(N->e->data->count(N->I) == 1 ){
              Fact * c = N->e->data->find(N->I)->second;
              if(c->possibleValues.size() == 1 && !c->isBottom){
                set<Value *>::iterator itl = c->possibleValues.begin();
                Value * result = *itl;
                BasicBlock::iterator ii(N->I);
                ReplaceInstWithValue(N->I->getParent()->getInstList(), ii, result);
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

char ConstantFoldingPass::ID = 0;
static RegisterPass<ConstantFoldingPass> X("ConstPass", "Const Pass",false,false);
