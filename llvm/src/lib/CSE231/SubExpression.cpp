#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CFG.h"
#include "llvm/IR/Constants.h"
#include "DFF.h"
#include <string>
#include <map>
#include <queue>
#include <list>
#include <set>

#include <iostream>
using namespace std;
using namespace llvm;

namespace {
    struct SubExpression : public FunctionPass {

        typedef map<string, set<string> > InstFact;
        static char ID;
        //enum to represent different instruction formats
        enum InstType {X_Eq_C, X_Eq_C_Op_Z, X_Eq_Y_Op_C, X_Eq_C_Op_C, X_Eq_Y_Op_Z, X_Eq_Y, X_Eq_Addr_Y, X_Eq_Star_Y, Star_X_Eq_Y, PHI, UNKNOWN};

        //std::queue<BasicBlock *, std::list<BasicBlock *> > bbQueue; //Worklist queue
        set<BasicBlock*> bbQueue;
        DFF<string> dataFlowFactsMap; //DataFlowFact map for all instructions


        SubExpression() : FunctionPass(ID){ }

        string createYopZString(Instruction* inst) {
            string temp;
            //get body of instruction
            raw_string_ostream t(temp);
            t<<(*inst);
            unsigned index = temp.find("=");
            if (index < temp.size()-2) {
                index += 2; //skip the "= "
                temp = temp.substr(index);
            }
            else temp.clear();

            return temp;
        }

        virtual void print(raw_ostream &O,const Module*) const;

        virtual bool runOnFunction (Function &F) {
            errs() << "RunOnFunction "<<(F.getName()).str()<<"\n";
            runWorkList(F);
            //dataFlowFactsMap.printEverything();
            /*for (Function::iterator blk = F.begin(), e = F.end(); blk != e; ++blk) {
                for (BasicBlock::iterator j = blk->begin(), e = blk->end(); j != e; ++j)
                    dataFlowFactsMap.printInsFact(j);
            }*/
            return true;
        }

        virtual bool flowFunction (Instruction* inst, 
                                   InstType instType, 
                                   InstFact* dff_in) {
            //inst->dump();
            //errs()<<"Before the instruction: ";
            //dataFlowFactsMap.printInsFact(dff_in); 
            
            switch (instType) {
                case PHI:
                {
                    //errs() << "PHI Node\n";
                    return dataFlowFactsMap.setInsFact(inst, dff_in);
                    break;
                }
                case X_Eq_Y_Op_Z:
                case X_Eq_Y_Op_C:
                case X_Eq_C_Op_Z:
                case X_Eq_C_Op_C:
                {
                    //errs() << "X = Y op Z\n";
                    //create a string of the form Y op Z
                    string entry = createYopZString(inst);
                    string variable = (string)inst->getName();
                    (*dff_in)[variable].insert(entry);
                    return dataFlowFactsMap.setInsFact(inst, dff_in);
                    break;
                }
                case X_Eq_Y:
                {
                    //It's safe to cast to StoreInst
                    StoreInst* si = dyn_cast<StoreInst>(inst);
                    // Get Y
                    Value* Y_var = si->getOperand(0);
                    string Y_var_name = (string) Y_var->getName();
                    // Look in map for Y
                    set<string> tempSet = (*dff_in)[Y_var_name];
                    // If it has result then insert X 
                    // in the map with same result
                    Value* X_var = si->getOperand(1);
                    string X_var_name = (string) X_var->getName();
                    (*dff_in)[X_var_name].insert(tempSet.begin(), tempSet.end());

                    return dataFlowFactsMap.setInsFact(inst, dff_in);
                    break;
                }
                case X_Eq_C:
                // {
                    //For CSE we just ignore this
                    // Value *firstOperand = instnst->getOperand(0);
                    // Value *secondOperand = instnst->getOperand(1);
                    // string variable = (string) secondOperand->getName();
                    // (*dff_in)[variable].insert(something);
                // }
                case X_Eq_Addr_Y: //mem2reg pass saves us from this
                case X_Eq_Star_Y: //mem2reg pass saves us from this
                case Star_X_Eq_Y: //mem2reg pass saves us from this
                {
                    return dataFlowFactsMap.setInsFact(inst, dff_in);
                    break;
                }
                case UNKNOWN:
                {
                    //errs() << "ERROR: Unknown Type\n";
                    return dataFlowFactsMap.setInsFact(inst, dff_in);
                    break;
                }
                default:
                    errs() << "ERROR: This type is not handled: "<<instType<<"\n";
            }
            return true;
        }

        virtual InstFact* join (InstFact* inst_a, InstFact* inst_b) {
            //errs() <<"Join\n";
            return dataFlowFactsMap.intersectFacts(inst_a, inst_b);
        }

        virtual void setBottom(Instruction* inst) {
            //errs() <<"SetBottom\n";
            //Bottom: Full Set
            dataFlowFactsMap.setFactFullSet(inst);
        }

        virtual void setTop(Instruction* inst) {
            //errs() << "setTop called with "<<inst<<"\n";
            dataFlowFactsMap.setFactEmptySet(inst);
        }
        
        InstFact* joinAllPredecessors(set<Instruction*> predecessors) {
            InstFact* result = new InstFact;
            //Check if any one is the empty set and return empty set
            bool first = true;
            //errs()<<"Set has size: "<<predecessors.size()<<"\n";
            for (set<Instruction*>::iterator it=predecessors.begin(); it!=predecessors.end(); ++it) {
                if (first) {
                    //errs()<<"Cloning first element: "<<*it<<"\n";
                    result = dataFlowFactsMap.getTempFact(*it);
                    first = false;
                }
                else {
                    //errs() <<"Joining instruction "<<*it<<"\n";
                    InstFact* if_tmp = dataFlowFactsMap.getTempFact(*it);
                    result = join(result,if_tmp); 
                }
            }
            return result;

            //copy the first element
            //loop through the rest elements
        }


        //returns instruction type for given instruction
        InstType getInstType(Instruction *inst){

            //PHI instruction - not sure what to do with it !
            if(dyn_cast<PHINode>(inst)){
                errs()<<"PHI\n";
                return PHI;
            }

            if (StoreInst *storeInst = dyn_cast<StoreInst>(inst)) {
                if (storeInst->getNumOperands() == 2) {
                    Value *firstOperand = storeInst->getOperand(0);
                    Value *secondOperand = storeInst->getOperand(1);
                    Type* t1 = firstOperand->getType();
                    if (dyn_cast<Constant>(firstOperand)) {
                        errs()<<"X_Eq_C\n";
                        return X_Eq_C;
                    }
                    
                    //Could be X_Eq_Y or X_Eq_Star_Y or Star_X_Eq_Y or X_Eq_Addr_Y
                    if (t1->isPointerTy()) {
                        errs()<<"X_Eq_Addr_Y\n";
                        return X_Eq_Addr_Y;
                    }
                    
                    //Could be X_Eq_Y or X_Eq_Star_Y or Star_X_Eq_Y
                    if (dyn_cast<BinaryOperator>(firstOperand)) {
                        errs()<<"X_Eq_Y\n";
                        return X_Eq_Y;
                    }

                    //Could be X_Eq_Star_Y or Star_X_Eq_Y
                    if (LoadInst* inst_t = dyn_cast<LoadInst>(firstOperand)) {
                        if (!(inst_t->getType())->isPointerTy()) {
                            if (!dyn_cast<LoadInst>(secondOperand)) {
                                errs()<<"X_Eq_Star_Y\n";
                                return X_Eq_Star_Y;
                            }
                            else {
                                errs()<<"Star_X_Eq_Y\n";
                                return Star_X_Eq_Y;
                            }
                        }
                    }
                }
            }

            //Will implement this when we extend to non-binary ops
            if (!inst->isBinaryOp()) {
                errs()<<"UNKNOWN\n";
                return UNKNOWN;
            }

            //check if one or both of the operands are constants
            User::op_iterator i = inst->op_begin();
            Constant *op1 = dyn_cast<Constant>(*i++);
            Constant *op2 = dyn_cast<Constant>(*i);

            if (!op1 && !op2){
                errs()<<"X_Eq_Y_Op_Z\n";
                return X_Eq_Y_Op_Z;
            }
            else if (!op2){
                errs()<<"X_Eq_Y_Op_C\n";
                return X_Eq_Y_Op_C;
            }
            else if (!op1){
                errs()<<"X_Eq_C_Op_Z\n";
                return X_Eq_C_Op_Z;
            }
            else{
                errs()<<"X_Eq_C_Op_C\n";
                return X_Eq_C_Op_C;
            }
        }

        //populate the worklist queue with all basic blocks at start and inits them to bottom
        void populateBBQueue(Function &F){
            for (Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++){
                //bbQueue.push(&*BB);
                bbQueue.insert(BB);
                for(BasicBlock::iterator I = BB->begin(), IE = BB->end(); I != IE; I++){
                    Instruction *inst = &*I;
                    setBottom(inst);
                }
            }
        }

        //The worklist algorithm that runs the flow on the entire procedure
        void runWorkList(Function &F){

            populateBBQueue(F);

            while (!bbQueue.empty()){
                //BasicBlock* BB = bbQueue.front();
                //bbQueue.pop();
                set<BasicBlock*>::iterator BB_it = bbQueue.begin();
                BasicBlock* BB = *BB_it;
                bbQueue.erase(BB_it);
                
                BasicBlock* uniqPrev = NULL;
                InstFact* if_in = new InstFact;
                Instruction* inst;
                bool isBBOutChanged;

                errs() << "Working on "<<(string)BB->getName()<<"\n";

                //check if BB has only one predecessor
                if ((uniqPrev = BB->getUniquePredecessor()) != NULL){
                    //errs()<<"UNIQUE "<<(string)uniqPrev->getName()<<"\n";
                    if_in = getBBInstFactOut(uniqPrev);
                }
                //if it has more than one predecessor then join them all
                else {
                    set<Instruction*> predecessors;
                    //errs()<<"MULTIPLE\n";
                    for (pred_iterator it = pred_begin(BB), et = pred_end(BB); it != et; ++it)
                    {
                        BasicBlock* prevBB = *it;
                        //errs()<<"PREDECESSOR: "<<(string)prevBB->getName()<<"\n";
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
                    inst->dump();
                    InstType instType = getInstType(I);
                    isBBOutChanged = flowFunction(I, instType, if_in);

                    if_in = dataFlowFactsMap.getTempFact(inst);
                    //if_in = dataFlowFactsMap.getInsFact(I);
                }

                //if the OUT of the last instruction in the bb has changed then add all successors to the worklist
                if (isBBOutChanged){
                    //errs()<<"Things changed. Scheduling successors\n";
                    for (succ_iterator it = succ_begin(BB), et = succ_end(BB); it != et; ++it)
                    {
                        BasicBlock* succBB = *it;
                        //errs()<<"Successor: "<<succBB->getName()<<"\n";
                        //bbQueue.push(succBB);
                        bbQueue.insert(succBB);
                    }
                }
            }
        }

        //get the instructionFact for the last instruction in the given basic block
        InstFact* getBBInstFactOut(BasicBlock *BB){
            Instruction *I;
            I = BB->getTerminator();

            //errs()<<"Terminator: "<<*I<<"\n";

            //return dataFlowFactsMap.getInsFact(NULL);
            //return dataFlowFactsMap.getInsFact(I);
            return dataFlowFactsMap.getTempFact(I);
        }


    };

    void SubExpression::print(raw_ostream &O,const Module*) const{
        O << "Common SubExpression elimination pass done!! \n";           
    }
}

char SubExpression::ID = 0;

static RegisterPass<SubExpression> X("SubExpression", "Propagate common sub-expressions", false, false);





