#define DEBUG_TYPE "RangePass"
#include "RangeAnalysis.cpp"


using namespace llvm;
using namespace range;

namespace {
	static IRBuilder<> builder(getGlobalContext());

  struct RangePass : public ModulePass {
    static char ID; // Pass identification, replacement for typeid

    //iterate through all instructions, fill map.
    RangePass() : ModulePass(ID) {
			//errs() << "Range Module Created" << "\n";
		}

    void printMinError(long long size, long long index){
      errs() << "!WARNING - Array Index may be out of bounds:\n";
      errs() << "--Access ["<< index <<"] for an array of size [" << size << "]\n";
    }

    void printMaxError(long long size, long long index){
      errs() << "!WARNING - Array Index may be Negative:\n";
      errs() << "--Access ["<< index <<"] for an array of size [" << size << "]\n";
    }

    bool runOnModule(Module &M){
      /**/
      //create the worklist object
      Worklist<EdgeFact, FlowFunctions, Lattice>* wl = new Worklist<EdgeFact, FlowFunctions, Lattice>();
      wl->init(M);
      wl->run();
      //print results...
      //M.dump();
      //wl->printTop();
      //wl->printBottom();

      vector<Value *> allocations;
      //Do Range Analysis here:
      for(map<int, BBNode<EdgeFact>*>::iterator it = wl->bbMap.begin(); it != wl->bbMap.end(); it++) {

        //TODO : track all allocations!
        BBNode<EdgeFact> * BBN = it->second;
        //For each node
        for(unsigned int i=0; i<BBN->nodes.size(); i++){
          Node<EdgeFact> * N = BBN->nodes[i];
          //llvm::Instruction::GetElementPtr
          if(N->I->getOpcode() == Instruction::GetElementPtr && isa<AllocaInst>(N->I->getOperand(0))){
            //get the first operand (the array)
            AllocaInst * op0 = dyn_cast<AllocaInst>(N->I->getOperand(0));
            //get the third operand (the array index)
            Value * op2 = N->I->getOperand(2);
            //do a lookup on the index operand
            ArrayType * arr = dyn_cast<ArrayType>(op0->getAllocatedType());
            //constant case: simple compare
            if(isa<ConstantInt>(op2)){
              long long access = dyn_cast<ConstantInt>(op2)->getSExtValue();
              //do quick evaluation here:
              if(access > ((long long)arr->getNumElements()-1)){
                printMinError(arr->getNumElements(),access);
              }
              if(access < 0){
                printMaxError(arr->getNumElements(),access);
              }
            } else {
              //not simple case: do a lookup on op2
              //errs()<<"NOT SIMPLE CASE! : " << *N->I << "\n";
              EdgeFact * e = N->e;
              Value * entry = NULL;
              //case 1: operand is in the dictionary
              if(e->data->count(op2) == 1){
                entry = op2;
              } else {
                //case 2:
                //  -a) operand is loaded from a variable that is in the dictionary
                //  -b) operand is loaded from a computed result (not tracked)
                if(isa<Instruction>(op2)){
                  //get the variable that op2 loads its value from
                  Value * tV = dyn_cast<Instruction>(op2)->getOperand(0);

                  Instruction * tI = dyn_cast<Instruction>(tV);
                  if(tI->getOpcode() == Instruction::GetElementPtr){
                    //foreach fact in data
                    Value * temp_entry;
                    map<Value *,Fact *>::iterator it;
                    for(it = e->data->begin(); it != e->data->end(); it++) {
                      Instruction * key = dyn_cast<Instruction>(it->first);
                      if(tI && key && key->getOpcode() == Instruction::Load){
                        //if first operand is identical to the
                        Instruction * root = dyn_cast<Instruction>(key->getOperand(0));
                        if(root && root->isIdenticalTo(tI)){
                          temp_entry = key;
                        }
                      }
                    }
                    //if match found...
                    if(temp_entry != NULL){
                      //do a lookup for that entry
                      if(e->data->count(temp_entry) == 1){
                        entry = temp_entry;
                      }
                    }
                  }
                }
              }
              //if a range was found for the entry, then do analysis
              if(entry != NULL){
                Range r = e->data->find(entry)->second->ranges.front();
                long long aMin = dyn_cast<ConstantInt>(r.min)->getSExtValue();
                long long aMax = dyn_cast<ConstantInt>(r.max)->getSExtValue();
                //long long m = numeric_limits<int>::max();
                //errs()<< "access min=" << aMin << " with max="<<aMax<<"\n";
                //if the minimum of the range is greater than array size, then we should throw an error
                if(aMin > ((long long)arr->getNumElements()-1-r.minIncl)){
                  //errs()<< "MIN IS GREATER THAN SIZE\n";
                  printMinError(arr->getNumElements(),aMin);
                }
                //if the maximum of the range is less than 0 then we should throw an error
                if(aMax < (1-(r.maxIncl ? 1 : 0))){
                  //errs()<< "MAX IS LESS THAN 0\n";
                  printMaxError(arr->getNumElements(),(aMax-(r.maxIncl ? 0 : 1)));
                }
              }
            }
          }
        }
      }
			return false;
    }
  };
}

char RangePass::ID = 0;
static RegisterPass<RangePass> X("RangePass", "Range Pass",false,false);
