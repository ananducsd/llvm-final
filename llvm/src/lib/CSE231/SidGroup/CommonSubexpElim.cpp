#define DEBUG_TYPE "CSEPass"
#include <map>
#include "AvailableExpression.cpp"


using namespace llvm;
using namespace avlexp;

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
      Worklist<EdgeFact, FlowFunctions, Lattice>* wl = new Worklist<EdgeFact, FlowFunctions, Lattice>(10010);
      wl->init(M);
      wl->run();
      //print results...
      //M.dump();
      //wl->printTop();
      //wl->printBottom();

      bool isChanged = true;
      while(isChanged == true){
        isChanged = false;
        for(map<int, BBNode<EdgeFact>*>::iterator it = wl->bbMap.begin(); it != wl->bbMap.end(); it++) {
          BBNode<EdgeFact> * BBN = it->second;
          //For each node
          for(unsigned int i=0; i<BBN->nodes.size(); i++){
          //aExpr
            Node<EdgeFact> * N = BBN->nodes[i];
            //update the current available expressions (remove any duplicates)
            for(set<Instruction*>::iterator it = N->e->aExpr.begin(); it != N->e->aExpr.end(); it++) {
              Instruction * compare = *it;
              //general case:
              if(compare->isIdenticalTo(N->I) && N->I->getOpcode() != Instruction::Alloca){
                //replace all instructions with the available expression!
                N->I->replaceAllUsesWith(compare);
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
