#include "llvm/Pass.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CFG.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include <string>
#include <map>
#include <queue>
#include <list>
#include <set>
#include "DFF.h"

using namespace llvm;
using namespace std;

//enum to represent different instruction formats
enum InstType {X_Eq_C, X_Eq_C_Op_Z, X_Eq_Y_Op_C, X_Eq_C_Op_C, X_Eq_Y_Op_Z, X_Eq_Y, X_Eq_Addr_Y, X_Eq_Star_Y, Star_X_Eq_Y, PHI, UNKNOWN};

namespace{
    struct ConstantProp : public FunctionPass
    {
        typedef map<string, set<int> > InstFact; //Instruction level data fact
        set<BasicBlock*> bbQueue;
        set<Instruction*> removeInstrSet;
        DFF<int> dataFlowFactsMap; //DataFlowFact map for all instructions

        static char ID;

        ConstantProp() : FunctionPass(ID) {}

        virtual void print(raw_ostream &O,const Module*) const;

        virtual bool runOnFunction(Function &F) {

            //call the worklist algorithm
            this->runWorkList(F);
            set<Instruction*>::iterator inst_it;

            for(inst_it = removeInstrSet.begin(); inst_it != removeInstrSet.end(); inst_it++) {
                errs() << "Trying to delete instruction " << (*inst_it)->getOpcodeName() << "\n";
                Instruction *temp = (*inst_it);
                temp->eraseFromParent();
            }

            /*
            for (Function::iterator blk = F.begin(), e = F.end(); blk != e; ++blk) {
                for (BasicBlock::iterator j = blk->begin(), e = blk->end(); j != e; ++j){
                    dataFlowFactsMap.printInsFact(j);
                }
            }*/

        return true;
    }

    // returns true if the flow function was changed
    bool flowFunction(Instruction* inst, InstFact* if_in)
    { 
        //this will be the value we will push
        Constant *C = NULL;
        // the first thing we need to do is check to see if it's a PHI node
        if(PHINode *PN = dyn_cast<PHINode>(inst))
        {
            return dataFlowFactsMap.setInsFact(inst, if_in);

            for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
                Value *Incoming = PN->getIncomingValue(i);
                // If the incoming value is undef then skip it.  Note that while we could
                // skip the value if it is equal to the phi node itself we choose not to
                // because that would break the rule that constant folding only applies if
                // all operands are constants.
                if (isa<UndefValue>(Incoming))
                    continue;
                // If the incoming value is not a constant, then give up.
                Constant *C2 = dyn_cast<Constant>(Incoming);
                if (!C2)
                {   
                    string opVarID = Incoming->getName();
                    if(DFF<int>::exists(if_in, opVarID))
                    {   
                        if( (*if_in)[opVarID].size() > 0 ) 
                        {   
                            // get the integer value
                            DFF<int>::SET_IT it = (*if_in)[opVarID].begin();
                            int val = *it;

                            // cast it as an unsigned value
                            //unsigned NumBits = 32; 
                            uint64_t uVal = (uint64_t) val;

                            // get the type information needed for LLVM then cast it as a constant int, first get it as an integer type, then create the constant from that...
                            //IntegerType* intType = IntegerType::get(getGlobalContext(), NumBits);
                            ConstantInt* newConstInt = ConstantInt::get(llvm::Type::getInt32Ty(getGlobalContext()), uVal, true);

                            Type* myType = llvm::Type::getPrimitiveType(getGlobalContext(), llvm::Type::IntegerTyID);
                            Constant* newConst = llvm::ConstantInt::get(myType, newConstInt->getValue());
                            C2 = newConst;

                        } else {
                            return dataFlowFactsMap.setInsFact(inst, if_in);
                        }
                    } else {
                        return dataFlowFactsMap.setInsFact(inst, if_in);
                    }
                }

                // Fold the PHI's operands.
                if (ConstantExpr *NewC = dyn_cast<ConstantExpr>(C2))
                {
                    errs() << "TODO: Got a constant expression, the vallue is: " << NewC << ". We don't handle this case\n";
                    //C = ConstantFoldConstantExpression(NewC, TD, TLI);
                }

                // If the incoming value is a different constant to
                // the one we saw previously, then give up.
                if (C2 && C != C2)
                    return dataFlowFactsMap.setInsFact(inst, if_in);
                C = C2;
            }
        } else {

            // the first thing we need to check is to see if all operands are constants
            SmallVector<Constant*, 8> Ops;

            for (User::op_iterator i = inst->op_begin(), e = inst->op_end(); i != e; ++i)
            {
                ConstantInt *Op = dyn_cast<ConstantInt>(*i);
                if (!Op)
                {
                    // this op isn't a constant, but we might have a mapping for it to a constant!
                    string opVarID = (string) (*i)->getName();
                    if(DFF<int>::exists(if_in, opVarID))
                    {
                        errs() << opVarID << " exists in the instruction fact, so let's check if it has a value.\n";
                        if( (*if_in)[opVarID].size() == 1 )
                        {
                            // get the integer value
                            DFF<int>::SET_IT it = (*if_in)[opVarID].begin();
                            int val = *it;

                            errs() << "    the value is: " << val << ".\n";
                            // cast it as an unsigned value
                            //unsigned NumBits = 32;
                            uint64_t uVal = (uint64_t) val;

                            // get the type information needed for LLVM then cast it as a constant int
                            //IntegerType* intType = IntegerType::get(getGlobalContext(), NumBits);
                            ConstantInt* newConst = ConstantInt::get(llvm::Type::getInt32Ty(getGlobalContext()), uVal, true);

                            Op = newConst; //TODO: make this so it actually assigns the correct operand value
                        } else {
                            errs() << "ERROR: We don't have any info for this instruction, but it exists in the map - ignore/weird case.\n";
                            return dataFlowFactsMap.setInsFact(inst, if_in);
                        }
                    } else
                    {
                        return dataFlowFactsMap.setInsFact(inst, if_in); // this op isn't constant, and it's not in our map - so there's nothing we can do with it. Ignore it.
                    }
                } else {
                    // This was a constant int, but we need to check if it was a Constant Expression
                    if (ConstantExpr *NewCE = dyn_cast<ConstantExpr>(Op))
                    {
                        errs() << "TODO: Got a constant expression, the vallue is: " << NewCE << ". We don't handle this case.\n";
                        return dataFlowFactsMap.setInsFact(inst, if_in); //TODO look at next line
                        //Op = constantFold(NewCE, DL, TLI); // this is what the original code does..
                    }
                }
                // now that we've set Op in above code, place it into the operand array
                Ops.push_back(Op);
            }

            // now that we have all constants, we will merge them and update the DFF
            // but we will only do it for certain kinds of instructions
            if (dyn_cast<CmpInst>(inst) || dyn_cast<LoadInst>(inst) || dyn_cast<InsertValueInst>(inst) || dyn_cast<ExtractValueInst>(inst))
            {
                return dataFlowFactsMap.setInsFact(inst, if_in);
            }

            // at this point all incoming values are the same and constant,
            // so look up the op and perform the constant propigation
            unsigned Opcode = inst->getOpcode();

            //first check if it's a binop
            if(Instruction::isBinaryOp(Opcode))
            {
                C = ConstantExpr::get(Opcode, Ops[0], Ops[1]);
            } else {
                // give up
                return dataFlowFactsMap.setInsFact(inst, if_in);
            }
        }

        // now we insert the constant into the instruction fact
        // but first we need to look up what variable it is in the map!
        string varID = (string) inst->getName();

        //now extract the value from the Constant type
        ConstantInt* constInt = dyn_cast<ConstantInt>(C);
        int inputInt = (int) constInt->getSExtValue();

        // now that we know the variable we kill the original
        if(DFF<int>::exists(if_in, varID))
        {
            //DFF<int>::removeVarFacts(if_in, varID);
            (*if_in)[varID].erase((*if_in)[varID].begin(), (*if_in)[varID].end());
        }

        // and insert it into the temp map
        (*if_in)[varID].insert(inputInt);

        // now we will actually replace the instruction with this constant!
        errs() << "Since we know this instruction (" << varID << ") evaluates to a constant, we will replace it with " << inputInt << ".\n";
        inst->replaceAllUsesWith(constInt);
        //inst->eraseFromParent(); insert instructions to be removed later
        removeInstrSet.insert(inst);

        // finally place this into the DFF and check if it differs from the previous
        return dataFlowFactsMap.setInsFact(inst, if_in);
    }

    InstFact* join (InstFact* a, InstFact* b)
    {
        return dataFlowFactsMap.intersectFacts(a,b);
    }

    void setBottom(Instruction* inst)
    {
        dataFlowFactsMap.setFactFullSet(inst);
    }

    void setTop(Instruction* inst)
    {
        dataFlowFactsMap.setFactEmptySet(inst);
    }

    //populate the worklist queue with all basic blocks at start and inits them to bottom
    void populateBBQueue(Function &F){
        for (Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++){
            bbQueue.insert(BB);

            for(BasicBlock::iterator I = BB->begin(), IE = BB->end(); I != IE; I++){
                Instruction *inst = &*I;
                setBottom(inst);
            }
        }
    }

    InstFact* joinAllPredecessors(set<Instruction*> predecessors) {
        InstFact* result = new InstFact;
        //Check if any one is the empty set and return empty set
        bool first = true;
        for (set<Instruction*>::iterator it=predecessors.begin(); it!=predecessors.end(); ++it) {
            if (first) {
                result = dataFlowFactsMap.getTempFact(*it);
                first = false;
            }
            else {
                InstFact* if_tmp = dataFlowFactsMap.getTempFact(*it);
                result = join(result,if_tmp);
            }
        }
        return result;
    }


    //The worklist algorithm that runs the flow on the entire procedure
    void runWorkList(Function &F){

        populateBBQueue(F);

        while (!bbQueue.empty()){
            set<BasicBlock*>::iterator BB_it = bbQueue.begin();
            BasicBlock* BB = *BB_it;
            bbQueue.erase(BB_it);

            BasicBlock* uniqPrev = NULL;
            InstFact* if_in = new InstFact;
            Instruction* inst;
            bool isBBOutChanged;

            //check if BB has only one predecessor
            if ((uniqPrev = BB->getUniquePredecessor()) != NULL){
                if_in = getBBInstFactOut(uniqPrev);
            }
            //if it has more than one predecessor then join them all
            else {
                set<Instruction*> predecessors;
                for (pred_iterator it = pred_begin(BB), et = pred_end(BB); it != et; ++it)
                {
                    BasicBlock* prevBB = *it;
                    Instruction* in_temp = prevBB->getTerminator();
                    if (dataFlowFactsMap.isFullSet(in_temp)) continue;
                    predecessors.insert(in_temp);
                }
                if_in = joinAllPredecessors(predecessors);
            }

            //run the flow function for each instruction in the basic block
            for (BasicBlock::iterator I = BB->begin(), IE = BB->end(); I != IE; I++){

                inst = &*I;
                string opname = inst->getOpcodeName();
                isBBOutChanged = flowFunction(I, if_in);

                if_in = dataFlowFactsMap.getTempFact(inst);
            }

            //if the OUT of the last instruction in the bb has changed then add all successors to the worklist
            if (isBBOutChanged){
                for (succ_iterator it = succ_begin(BB), et = succ_end(BB); it != et; ++it)
                {
                    BasicBlock* succBB = *it;
                    bbQueue.insert(succBB);
                }
            }
        }
    }

    //get the instructionFact for the last instruction in the given basic block
    InstFact* getBBInstFactOut(BasicBlock *BB){
        Instruction *I;
        I = BB->getTerminator();

        return dataFlowFactsMap.getTempFact(I);
    }
  };

    void ConstantProp::print(raw_ostream &O,const Module*) const{
        O << "Const prop pass done!! \n";           
    }
}

char ConstantProp::ID = 0;

static RegisterPass<ConstantProp> X("ConstantProp", "Propigate constant values", false, false);
