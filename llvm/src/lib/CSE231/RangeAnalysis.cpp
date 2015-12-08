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

#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FEnv.h"
#include "llvm/Support/MathExtras.h"
#include <limits>
#include "DFF.h"

using namespace llvm;
using namespace std;

//enum to represent different instruction formats
enum InstType {X_Eq_C, X_Eq_C_Op_Z, X_Eq_Y_Op_C, X_Eq_C_Op_C, X_Eq_Y_Op_Z, X_Eq_Y, X_Eq_Addr_Y, X_Eq_Star_Y, Star_X_Eq_Y, PHI, UNKNOWN};

//define +/- infinity
int MAX = std::numeric_limits<int>::max();
int MIN = std::numeric_limits<int>::min();

namespace{
    struct RangeAnalysis : public FunctionPass
    {
        typedef map<string, set<int> > InstFact; //Instruction level data fact, not we will define position 0 to be the min and position 1 to be the max. 
        set<BasicBlock*> bbQueue;
        DFF<int> dataFlowFactsMap; //DataFlowFact map for all instructions

        static char ID;
        
        RangeAnalysis() : FunctionPass(ID) {}

        virtual void print(raw_ostream &O,const Module*) const;
        
        virtual bool runOnFunction(Function &F) {

            //call the worklist algorithm
            this->runWorkList(F);

            return true;
        }

        bool pushValue(Instruction* inst, InstFact* if_in, ConstantInt* C)
        {
            string varID = (string) inst->getName();

            //now extract the value from the Constant type
            ConstantInt* constInt = dyn_cast<ConstantInt>(C);
            int inputInt = (int) constInt->getSExtValue(); 

            // now that we know the variable we kill the original
            if(DFF<int>::exists(if_in, varID))
            {
                (*if_in)[varID].erase((*if_in)[varID].begin(), (*if_in)[varID].end());
            }
            errs() << "Adding/updating the range for " << varID << " to be: (" << inputInt << "," << inputInt+1 << ").\n";
            
            // and insert it into the temp map, since we know it's exact value it's max and min will be identical, ie: the range spans just one value
            (*if_in)[varID].insert(inputInt);
            (*if_in)[varID].insert(inputInt+1); //sets don't allow duplicates, so we need to add 1, this is ok. We lose some precision, but still correct

            // finally place this into the DFF and check if it differs from the previous 
            return dataFlowFactsMap.setInsFact(inst, if_in);
        }

        bool pushValue(Instruction* inst, InstFact* if_in, int min, int max)
        {
            string varID = (string) inst->getName();
            if(min == max)
                max++;

//            errs() << "Adding/updating the range for " << varID << " to be: (" << min << "," << max << ").\n";
           
            // now that we know the variable we kill the original
            if(DFF<int>::exists(if_in, varID))
            {
                (*if_in)[varID].erase((*if_in)[varID].begin(), (*if_in)[varID].end());
            }
            
            // and insert it into the temp map, since we know it's exact value it's max and min will be identical, ie: the range spans just one value
            (*if_in)[varID].insert(min);
            (*if_in)[varID].insert(max);

            // finally place this into the DFF and check if it differs from the previous 
            if(min == MIN && max == MAX)
            {
                //errs() << "setting instruction to fullset!!!\n\n";
                dataFlowFactsMap.setFactFullSet(inst);
                return dataFlowFactsMap.setInsFact(inst, if_in, false);
            }
        
            return dataFlowFactsMap.setInsFact(inst, if_in);
        }


        // returns true if the flow function was changed
        bool flowFunction(Instruction* inst, InstFact* if_in)
        {  
            // set these to reverse values to guarantee they will be overwritten when we compare them
            int newMax = MIN;
            int newMin = MAX;

            // the first thing we need to do is check to see if it's a PHI node
            if(PHINode *PN = dyn_cast<PHINode>(inst))
            {
                for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) 
                {
                    Value *Incoming = PN->getIncomingValue(i);
                    // If the incoming value is undef then skip it.  Note that while we could
                    // skip the value if it is equal to the phi node itself we choose not to
                    // because that would break the rule that constant folding only applies if
                    // all operands are constants.
                    if (isa<UndefValue>(Incoming))
                        continue;
                    // If the incoming value is not a constant, then look it up in our DFF to see if it exists!.
                    Constant *C = dyn_cast<Constant>(Incoming);
                    if (!C)
                    {
                        string opVarID = Incoming->getName();

                        // check to see if we have a range mapping for it in the instruction fact
                        if(DFF<int>::exists(if_in, opVarID))
                        {
                            if( (*if_in)[opVarID].size() > 0 )
                            {
                                // get the integer value
                                DFF<int>::SET_IT it = (*if_in)[opVarID].begin();
                                if( *it < newMin )
                                    newMin = *it;

                                if( *(++it) > newMax)
                                    newMax = *(it);

                                continue;
                            } else {

                                // it's not a constant... and we don't have mapping for it
                                return pushValue(inst, if_in, MIN, MAX);
                            } // no need to look at the other incoming nodes, as we can't go below MIN or above MAX (ie: +/- infinity)
                        } else {
                            // it's not a constant... and we don't have a mapping for it
                            return pushValue(inst, if_in, MIN, MAX);
                        } // no need to look at the other incoming nodes, as we can't go below MIN or above MAX (ie: +/- infinity)


                        // Fold the PHI's operands.
                        if (dyn_cast<ConstantExpr>(C))
                        {
                            // TODO: This isn't handled.
                            return pushValue(inst, if_in, MIN, MAX);
                            //C = ConstantFoldConstantExpression(NewC, TD, TLI);
                        }

                    } else { // otherwise the phi value evaluated to be a constant, we need to check if it's greater than the current max, or less than the current min.
                        // first we need to get the integer representation
                        if(ConstantInt* constInt = dyn_cast<ConstantInt>(C))
                        {
                            int inputInt = (int) constInt->getSExtValue(); 

                            // now update as needed
                            if( inputInt < newMin )
                                newMin = inputInt;

                            if( inputInt > newMax)
                                newMax = inputInt;
                        } else
                        {
                            return pushValue(inst, if_in, MIN, MAX);
                        }
                    }
                }
            } else if(dyn_cast<CmpInst>(inst)) // special case number 2, 
            {

            } else // special case number 3, it's a regular instruction with either constant values or ranges we can look up in our DFF
            {
                // the first thing we need to check is to see if all operands are constants, or exist in our DFF
                SmallVector<Constant*, 8> minOps;
                SmallVector<Constant*, 8> maxOps;
                  
                for (User::op_iterator i = inst->op_begin(), e = inst->op_end(); i != e; ++i) 
                {
                    ConstantInt *minOp = dyn_cast<ConstantInt>(*i);
                    ConstantInt *maxOp = dyn_cast<ConstantInt>(*i);
                    if (!minOp)
                    {
                        // this op isn't a constant, but we might have a mapping for it to a constant!
                        string opVarID = (string) (*i)->getName();
                        if(DFF<int>::exists(if_in, opVarID))
                        {
                            if( (*if_in)[opVarID].size() > 0 )
                            {
                                // get the integer value
                                DFF<int>::SET_IT it = (*if_in)[opVarID].begin();
                                int minVal = *it;

                                // now increment and get the max value stored
                                ++it;
                                int maxVal = *it;

                                // cast it as an unsigned value
                                //unsigned NumBits = 32;
                                uint64_t uMinVal = (uint64_t) minVal;
                                uint64_t uMaxVal = (uint64_t) maxVal;

                                // get the type information needed for LLVM then cast it as a constant int
                                //IntegerType* intType = IntegerType::get(getGlobalContext(), NumBits);
                                minOp = ConstantInt::get(llvm::Type::getInt32Ty(getGlobalContext()), uMinVal, true);
                                maxOp = ConstantInt::get(llvm::Type::getInt32Ty(getGlobalContext()), uMaxVal, true);
                                
                                // use following code to go from ConstantInt type to Constant type.
                                //Type* myType = llvm::Type::getPrimitiveType(getGlobalContext(), llvm::Type::IntegerTyID);
                                //Constant* newConst = llvm::ConstantInt::get(myType, newConstInt->getValue());

                                if( minVal != MIN || maxVal != MAX)
                                    errs() << "~~~ This item (" << opVarID << ") was in the set, it's min value: " << minVal << ", and it's max value: " << maxVal << "\n";
                            } else { // give up
                                return pushValue(inst, if_in, MIN, MAX);
                            }
                        } else { // give up                     
                            return pushValue(inst, if_in, MIN, MAX);
                        }
                    } else {
                        // This was a constant int, but we need to check if it was a Constant Expression
                        if (dyn_cast<ConstantExpr>(minOp))
                        {
                            // give up
                            return pushValue(inst, if_in, MIN, MAX);
                            //Op = constantFold(NewCE, DL, TLI); // this is what the original code does.. 
                        }
                    }
                    // now that we've set Op in above code, place it into the operand array
                    // NOTE: if this is an actual constant, then they will be the same value, and the range will be just the 1 value!
                    minOps.push_back(minOp);
                    maxOps.push_back(maxOp);
                }


                if (dyn_cast<LoadInst>(inst) || dyn_cast<InsertValueInst>(inst) || dyn_cast<ExtractValueInst>(inst)) 
                {
                    // give up
                    return pushValue(inst, if_in, MIN, MAX);
                }
                            
                // look up the op and perform the constant propigation
                unsigned Opcode = inst->getOpcode();
                
                //first check if it's a binop
                if(Instruction::isBinaryOp(Opcode))
                {
                    if(isa<ConstantExpr>(minOps[0]) || isa<ConstantExpr>(minOps[1]))
                    {
                        return pushValue(inst, if_in, MIN, MAX);
                    }

                    // make sure the optypes are the same, if they aren't we can't do anything.
                    if(minOps[0]->getType() != minOps[1]->getType() || minOps[0]->getType() != maxOps[0]->getType() || minOps[0]->getType() != maxOps[1]->getType())
                        return pushValue(inst, if_in, MIN, MAX);
                    // there will be 4 combinations, we need to enumerate 
                    Constant* c1 = ConstantExpr::get(Opcode, minOps[0], minOps[1]);
                    Constant* c2 = ConstantExpr::get(Opcode, minOps[0], maxOps[1]);
                    Constant* c3 = ConstantExpr::get(Opcode, maxOps[0], maxOps[1]);
                    Constant* c4 = ConstantExpr::get(Opcode, maxOps[0], minOps[1]);

                    // now we need to convert these to integers to compare!
                    if((dyn_cast<ConstantInt>(c1)) && (dyn_cast<ConstantInt>(c2)) && (dyn_cast<ConstantInt>(c3)) && (dyn_cast<ConstantInt>(c4)))
                    {
                        int inputInt1 = (int) (dyn_cast<ConstantInt>(c1))->getSExtValue(); 
                        int inputInt2 = (int) (dyn_cast<ConstantInt>(c2))->getSExtValue(); 
                        int inputInt3 = (int) (dyn_cast<ConstantInt>(c3))->getSExtValue(); 
                        int inputInt4 = (int) (dyn_cast<ConstantInt>(c4))->getSExtValue(); 

                        // now determine which binop would produce the smallest value, if this is smaller than the current smallest range replace it!
                        if(inputInt1 < newMin)
                            newMin = inputInt1;
                        if(inputInt2 < newMin)
                            newMin = inputInt2;
                        if(inputInt3 < newMin)
                            newMin = inputInt3;
                        if(inputInt4 < newMin)
                            newMin = inputInt4;

                        // now determine which binop would produce the largest value, if this is greater than the current largest range replace it!
                        if(inputInt1 > newMax)
                            newMax = inputInt1;
                        if(inputInt2 > newMax)
                            newMax = inputInt2;
                        if(inputInt3 > newMax)
                            newMax = inputInt3;
                        if(inputInt4 > newMax)
                            newMax = inputInt4;
                    } else { // give up
                        return pushValue(inst, if_in, MIN, MAX);
                    }

                } else {
                    // give up, we are only handling binops
                    return pushValue(inst, if_in, MIN, MAX);
                }
            }

            // finally, since we've set up the min/max values for this appropriatly, we set it into the DFF!
            return pushValue(inst, if_in, newMin, newMax);
        }

        InstFact* join (InstFact* a, InstFact* b)
        {  
            return dataFlowFactsMap.unionFacts(a,b, true);
        }   

        void setBottom(Instruction* inst)
        {   
            dataFlowFactsMap.setFactEmptySet(inst);
        }   

        void setTop(Instruction* inst)
        {  
            dataFlowFactsMap.setFactFullSet(inst);
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

    void RangeAnalysis::print(raw_ostream &O,const Module*) const{
        O << "RangeAnalysis pass done!! \n";           
    }
}

char RangeAnalysis::ID = 0;

static RegisterPass<RangeAnalysis> X("RangeAnalysis", "Performs an analysis on the variables in the program, returning the known limits to the possible range it may take", false, false);
