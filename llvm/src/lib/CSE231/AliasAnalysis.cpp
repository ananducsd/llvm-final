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
    struct AliasAnalysis : public FunctionPass {

        typedef map<string, set<string> > InstFact;
        static char ID;
        //enum to represent different instruction formats
        enum InstType {X_Eq_NEW, X_Eq_C, X_Eq_C_Op_Z, X_Eq_Y_Op_C, X_Eq_C_Op_C, X_Eq_Y_Op_Z, X_Eq_Y, X_Eq_Addr_Y, X_Eq_Star_Y, Star_X_Eq_Y, PHI, UNKNOWN};

        //std::queue<BasicBlock *, std::list<BasicBlock *> > bbQueue; //Worklist queue
        set<BasicBlock*> bbQueue;
        DFF<string> dataFlowFactsMap; //DataFlowFact map for all instructions


        AliasAnalysis() : FunctionPass(ID){ }

        string createInstString(Instruction* inst) {
            string temp;
            //get body of instruction
            raw_string_ostream t(temp);
            t<<(*inst);

            return temp;
        }

        virtual void print(raw_ostream &O,const Module*) const;
        virtual bool runOnFunction (Function &F) {
            errs() << "############ RunOnFunction "<<(F.getName()).str()<<" ##############\n";
            runWorkList(F);
            dataFlowFactsMap.myfile.open("outfromc.txt");
            //dataFlowFactsMap.printEverything();

            errs()<<"########### ALIAS ANALYSIS OUTPUT #########\n";

            for (Function::iterator blk = F.begin(), e = F.end(); blk != e; ++blk) {
                for (BasicBlock::iterator j = blk->begin(), e = blk->end(); j != e; ++j)
                    dataFlowFactsMap.printInsFact(j);
            }
            return true;
        }

        virtual bool flowFunction (Instruction* inst,
                InstType instType,
                InstFact* dff_in) {
            //inst->dump();
            //errs()<<"Before the instruction: ";
            //dataFlowFactsMap.printInsFact(dff_in);

            switch (instType) {
                case X_Eq_NEW:
                    {
                        string var = (string)inst->getName();
                        string summary = "S(" + var + ")";
                        (*dff_in)[var].insert(summary);
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
                case X_Eq_Addr_Y:
                    {
                        GetElementPtrInst* gepInst = dyn_cast<GetElementPtrInst>(inst);
                        Value* Y_var_1 = gepInst->getOperand(0);
                        string Y_var_name_1 = (string) Y_var_1->getName();
                        string X_var_name_1 = inst->getName();

                        // Look in map for Y
                        set<string> tempSet1 = (*dff_in)[Y_var_name_1];
                        (*dff_in)[X_var_name_1].insert(tempSet1.begin(), tempSet1.end());

                        //LoadInst *li = dyn_cast<LoadInst>(inst);
                        //Value* Y_var_1 = li->getOperand(0);
                        //string Y_var_name_1 = (string) Y_var_1->getName();

                        //if (Y_var_name_1 != ""){
                        //string str_inst = createInstString(inst);
                        //int pos = str_inst.find("%") + 1;
                        //int pos1 = str_inst.find(" = ");
                        //string X_var_name_1 = str_inst.substr(pos, pos1 - pos);
                        //// Look in map for Y
                        //set<string> tempSet1 = (*dff_in)[Y_var_name_1];

                        //(*dff_in)[X_var_name_1].insert(tempSet1.begin(), tempSet1.end());
                        //}
                        return dataFlowFactsMap.setInsFact(inst, dff_in);
                        break;
                    }
                case PHI:
                    {
                        //errs()<<"PHI :: I : "<<inst->getName()<<", X : "<<inst->getOperand(0)->getName()<<", Y : "<<inst->getOperand(1)->getName()<<"\n";
                        string phi_name = inst->getName();
                        string X_var_name_2 = inst->getOperand(0)->getName();
                        string Y_var_name_2 = inst->getOperand(1)->getName();

                        set<string> tempSet2 = (*dff_in)[X_var_name_2];
                        set<string> tempSet3 = (*dff_in)[Y_var_name_2];
                        (*dff_in)[phi_name].insert(tempSet2.begin(), tempSet2.end());
                        (*dff_in)[phi_name].insert(tempSet3.begin(), tempSet3.end());

                        return dataFlowFactsMap.setInsFact(inst, dff_in);
                        break;
                    }
                case X_Eq_Y_Op_Z:
                case X_Eq_Y_Op_C:
                case X_Eq_C_Op_Z:
                case X_Eq_C_Op_C:
                case X_Eq_C:
                case X_Eq_Star_Y: //mem2reg pass saves us from this
                case Star_X_Eq_Y: //mem2reg pass saves us from this
                    {
                        return dataFlowFactsMap.setInsFact(inst, dff_in);
                        break;
                    }
                case UNKNOWN:
                    {
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
            return dataFlowFactsMap.unionFacts(inst_a, inst_b);
        }

        virtual void setBottom(Instruction* inst) {
            //errs() <<"SetBottom\n";
            //Bottom: Full Set
            dataFlowFactsMap.setFactEmptySet(inst);
        }

        virtual void setTop(Instruction* inst) {
            //errs() << "setTop called with "<<inst<<"\n";
            dataFlowFactsMap.setFactFullSet(inst);
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

            if (dyn_cast<AllocaInst>(inst)){
                errs()<<"X_Eq_NEW\n";
                return X_Eq_NEW;
            }

            if (isa<GetElementPtrInst>(inst)){
                //Value *firstOp = loadInst->getOperand(0);
                //Type *t = firstOp->getType();
                //if (t->isPointerTy()){
                errs()<<"X_Eq_Addr_Y\n";
                return X_Eq_Addr_Y;
                //}
            }

            //if ( StoreInst *storeInst = dyn_cast<StoreInst>(inst)) {
            if (false){
                StoreInst *storeInst = dyn_cast<StoreInst>(inst);
                if (storeInst->getNumOperands() == 2) {
                    Value *firstOperand = storeInst->getOperand(0);
                    Value *secondOperand = storeInst->getOperand(1);
                    Type* t1 = firstOperand->getType();
                    Type* t2 = secondOperand->getType();

                    if (dyn_cast<Constant>(firstOperand)) {
                        errs()<<"X_Eq_C\n";
                        return X_Eq_C;
                    }

                    //Could be X_Eq_Y or X_Eq_Star_Y or Star_X_Eq_Y
                    if (t1->isPointerTy() && t2->isPointerTy()) {
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
                //                errs()<<"UNKNOWN\n";
                return UNKNOWN;
            }
            else{

                //check if one or both of the operands are constants
                Constant *op1 = dyn_cast<Constant>(inst->getOperand(0));
                Constant *op2 = dyn_cast<Constant>(inst->getOperand(1));

                if (!op1 && !op2){
                    errs()<<"X_Eq_Y_Op_Z\n";
                    return X_Eq_Y_Op_Z;
                }
                else if (!op1){
                    errs()<<"X_Eq_Y_Op_C\n";
                    return X_Eq_Y_Op_C;
                }
                else if (!op2){
                    errs()<<"X_Eq_C_Op_Z\n";
                    return X_Eq_C_Op_Z;
                }
                else{
                    errs()<<"X_Eq_C_Op_C\n";
                    return X_Eq_C_Op_C;
                }
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

        InstFact* addFunctionArgsToInstructionMap (Instruction *inst, Function &F){
            Argument *arg = NULL;
            InstFact *if_in =  dataFlowFactsMap.getTempFact(inst);

            for(Function::arg_iterator AR = F.arg_begin(), AE = F.arg_end(); AR != AE; AR++){
                arg = &*AR;
                Type* ty = arg->getType();
                if (ty->isPointerTy()){
                    string arg_name = arg->getName();
                    string summary = "S(" + arg_name + ")";
                    (*if_in)[arg_name].insert(summary);
                }
            }

            return if_in;
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

                errs() << "########### Working on "<<(string)BB->getName()<<" ###############\n";

                //check if BB has only one predecessor
                if ((uniqPrev = BB->getUniquePredecessor()) != NULL){
                    //errs()<<"UNIQUE PREDECESSOR "<<(string)uniqPrev->getName()<<"\n";
                    if_in = getBBInstFactOut(uniqPrev);
                }
                //if it has NONE then use function args; if i t has many then join them all
                else {
                    pred_iterator it = pred_begin(BB), et = pred_end(BB);

                    if (it == et){
                        //errs()<<"########### NO PREDECESSORS ##############\n";
                        //Add function arguments to the map for the first instruction of the first basic block
                        Instruction* firstInst = (&*BB->begin());
                        if_in = addFunctionArgsToInstructionMap (firstInst, F);
                    }
                    else{
                        //errs()<<"########### MULTIPLE PREDECESSORS ##############\n";
                        set<Instruction*> predecessors;
                        for (;it != et; ++it)
                        {
                            BasicBlock* prevBB = *it;
                            errs()<<"PREDECESSOR: "<<(string)prevBB->getName()<<"\n";
                            Instruction* in_temp = prevBB->getTerminator();
                            if (dataFlowFactsMap.isFullSet(in_temp)){

                            }
                            predecessors.insert(in_temp);
                        }
                        if_in = joinAllPredecessors(predecessors);
                    }
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
    }
    void AliasAnalysis::print(raw_ostream &O,const Module*) const{
        O << "Alias Analysis pass done!! \n";
    }

    char AliasAnalysis::ID = 0;

    static RegisterPass<AliasAnalysis> X("alias_analysis", "Pointer analysis", false, false);





