#include <set>
#include <algorithm>
#include <iterator>
#include "llvm/IR/Value.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/CFG.h"
#include "stdio.h"
#include "string.h"
#include <vector>

using namespace std;
using namespace llvm;

class const_Fact
{
  public:
  bool isBottom; //everything defaults to bottom until a value is added
  set<Value *> possibleValues;
  const_Fact() {
    isBottom = true;
  }
  const_Fact(set<Value *> startValues) {
    if(startValues.size() > 0) { 
      possibleValues = startValues;
      isBottom = false;
      checkIfSuperconstant();
    } else {
      isBottom = true;
    }
  }
  const_Fact(const_Fact * parent) {
    //check case where the values aren't defined yet
    if(parent != NULL){
      isBottom = parent->isBottom;
      possibleValues = set<Value *>(parent->possibleValues);
    } else {
      isBottom = true;
    }
  }
  void checkIfSuperconstant(){
    if(possibleValues.size() > 1 || isBottom){
      possibleValues.clear();
      isBottom = true;
    }
  }
};

class const_Edge
{
  public:
  bool isTop;
  bool isBottom;
  map<Value *,const_Fact *> * data;

  const_Edge(){
    isTop = false;
    isBottom = true;
    data = new map<Value *,const_Fact *>();
  }
  //copy constructor for edge
  const_Edge(const_Edge * parent){
    isTop = parent->isTop;
    isBottom = parent->isBottom;
    data = new map<Value *,const_Fact *>();
    map<Value *,const_Fact *>::iterator it;
    for(it = parent->data->begin(); it != parent->data->end(); it++) {
      addFact(it->first,it->second);
    }
  }

  //checks if the value IS valuable data, or HAS valuable data associated with it.
  bool hasUsefulData(Value * test){
    if(dyn_cast<llvm::Constant>(test)){
      return true;
    }
    return data->count(test);
  }
  //lookup is useful for getting a keys fact, and creating a new one if not found on insertion
  const_Fact * lookup(Value * key){
    if(data->count(key) == 1) { //if key is instruction that has data already,
      return new const_Fact(data->find(key)->second); //then return that data
    } else {
      const_Fact * temp = new const_Fact(); //the key is not an instruction
      temp->isBottom = false;   //create a new fact to store the value
      temp->possibleValues.insert(key);
      temp->checkIfSuperconstant();
      return temp;
    }
  }
  void addFact(Value * key, const_Fact * f){ //can add multiple values, preserves bottom state
    if(data->count(key) == 0) {
      data->insert(pair<Value *,const_Fact *>(key,new const_Fact(f)));
    } else {
      const_Fact * ef = data->find(key)->second;
      ef->isBottom = (ef->isBottom || f->isBottom);
      if(!ef->isBottom){
        ef->possibleValues.insert(f->possibleValues.begin(),f->possibleValues.end());
      }
      ef->checkIfSuperconstant();
    }
  }
  void addValue(Value * key, Value * possibleValue) {
    const_Fact * f;
    if(data->count(key) == 0) { //check if key w/ fact exists
      f = new const_Fact();
      f->isBottom = false;
      data->insert(pair<Value *,const_Fact *>(key,f));
    } else {
      f = data->find(key)->second;
    }
    f->possibleValues.insert(possibleValue);
    f->checkIfSuperconstant();
    isTop = false;
  }
  void removeValue(Value * key, Value * possibleValue) {
    if(data->count(key) == 1) { //check that key exists
      const_Fact * f = data->find(key)->second;
      if(f->possibleValues.count(possibleValue)) { //check that value to remove exists
        f->possibleValues.erase(f->possibleValues.find(possibleValue));
      }
    }
  }
  void kill(Value * key) {
    if(data->count(key) == 1) { //check that key exists
      data->erase(data->find(key));
    }
  }
  void setValue(Value * key, const_Fact * destPointer){
    if(data->count(key) == 1) { //free old fact and set it equal to destPointer
      //free(data->find(key)->second); //WARNING : this might be a memory leak, but is unsafe to use
      data->find(key)->second = destPointer;
    } else { //if key does not exist, just create a new pair
      data->insert(pair<Value *,const_Fact *>(key,destPointer));
    }
  }
};

class const_Node
{
  public:
  Instruction * oI;
  const_Edge * e;
  
  const_Node(Instruction * I){
    e = new const_Edge();
    oI = I;
  }
};

class const_BBNode
{
  public:
  int bID;
  BasicBlock * originalBB;
  const_Edge * incoming;
  //can have multiple outgoing edges
  map<int, const_Edge *> outgoing;
  vector<const_Node *> nodes;
  
  const_BBNode(int ibID, BasicBlock * BB){
    bID = ibID;
    originalBB = BB;
    incoming = new const_Edge();
    //loop over each instruction in orginalBB and create a const_Node to point to them
    for (BasicBlock::iterator I = originalBB->begin(), Ie = originalBB->end(); I != Ie; ++I) {
      nodes.push_back(new const_Node(I));
    }
  }
  //use the names of the successor BBs to add the outgoing edges
  void setup() {
    for (succ_iterator SI = succ_begin(originalBB), E = succ_end(originalBB); SI != E; ++SI) {
      BasicBlock *Succ = *SI;
      int id = atoi(Succ->getName().str().c_str());
      outgoing[id] = new const_Edge();
    }
  }
};

class const_FlowFunctions
{
  public:
  const_FlowFunctions(){
    //errs() << "Flow Functions Created" << "\n";
  }
  
  map<int, const_Edge *> m(const_Edge * in, const_BBNode * N){
    map<int, const_Edge *> result;
    //go over each instruction in the BB and apply the flow function!!!
    for(unsigned int i=0;i<N->nodes.size() - 1;i++){
      in = mapInstruction(in, N->nodes[i]);
    }
    in->isTop = false;
    
    //use the terminating instructions to make an edge for each successor
    for (succ_iterator SI = succ_begin(N->originalBB), E = succ_end(N->originalBB); SI != E; ++SI) {
      BasicBlock *Succ = *SI;
      int id = atoi(Succ->getName().str().c_str());
      result[id] = mapTerminator(new const_Edge(in),N->nodes.back());
    }
    return result;
  }

  //do flow function calls here
  const_Edge * mapInstruction(const_Edge *in, const_Node *node) {
    switch(node->oI->getOpcode()) {
      //X = *
      case Instruction::Alloca: { //done
        //due to SSA, we don't need to check this one
        const_Fact * newFact = new const_Fact(); //it defaults to bottom = true
        in->data->insert(pair<Value *,const_Fact *>(node->oI,newFact));
        in->isTop = false;
      } break;
      //X* = Y
      case Instruction::Store: {
        //TODO : do a check that it isn't bottom in this one before killing it...
        if(in->data->count(node->oI->getOperand(1))) {
          //const_Fact * t = in->data->find(node->oI->getOperand(1))->second;
          //if(!t->isBottom){
            in->kill(node->oI->getOperand(1)); //remove all references to X
          //}
        }
        const_Fact * toAdd = in->lookup(node->oI->getOperand(0)); //generate the new fact for that operand
        in->addFact(node->oI->getOperand(1), toAdd); //update that fact
      } break;
      //X = Y
      case Instruction::Load: { //working on!
        //due to SSA we can just add a fact where it has the value pointing to the Y value...
        const_Fact * newFact;  
        if(in->data->count(node->oI->getOperand(0)) == 1) {
          newFact = new const_Fact(in->data->find(node->oI->getOperand(0))->second);
        } else {
          newFact = new const_Fact(); //case where there is no fact for Y
        }
        in->setValue(node->oI,newFact);
        in->isTop = false;
      } break;
      default : {
        if(isa<CallInst>(node->oI)){
          handleFuncCall(in,node->oI);
          return in;
        }
        const_Fact * newFact;
        set<Value *> results;
        //NOTE: we currently only support 2 operand other instructions
        if(node->oI->getNumOperands() == 2){
          if(in->hasUsefulData(node->oI->getOperand(0)) && in->hasUsefulData(node->oI->getOperand(1))){
            results = getAllResults(node->oI,in);
            if(results.size() == 0){
              newFact = new const_Fact();
            } else {
              newFact = new const_Fact(results);
            }
          } else { //set value to bottom if an operand is unusable
            newFact = new const_Fact();
          }
        } else {
          newFact = new const_Fact();
        }
        //update edge with fact
        in->setValue(node->oI,newFact);
        in->isTop = false;
        newFact->checkIfSuperconstant();
      } break;
    }
    //save incoming data to the node
    free(node->e);
    node->e = new const_Edge(in);
    return in;
  }
  set<Value *> getAllResults(Instruction * I, const_Edge * e) {
    set<Value *> results;
    set<Value *> bad;
    //step 1: cast the values as facts
    const_Fact * f1 = e->lookup(I->getOperand(0));
    const_Fact * f2 = e->lookup(I->getOperand(1));
    //use the opcode to determine the result -> use 
    set<Value *>::iterator it1,it2;
    for(it1 = f1->possibleValues.begin(); it1 != f1->possibleValues.end(); it1++) {
      for(it2 = f2->possibleValues.begin(); it2 != f2->possibleValues.end(); it2++) {
        Constant * c1 = dyn_cast<Constant>(*it1);
        Constant * c2 = dyn_cast<Constant>(*it2);
        if(c1 && c2){
          Constant * result = compute(c1,c2,I);
          if(result == NULL){return bad;}
          results.insert(result);
        } else {
          return bad;
        }
      }
    }
    return results;
  }
  //TODO : handle more opcodes!
  Constant * compute(Constant * c1, Constant * c2, Instruction * I){
    int opCode = I->getOpcode();
    switch(opCode){
      case Instruction::Add: {
        return ConstantExpr::getAdd(c1,c2);
      } break;
      case Instruction::FAdd: {
        return ConstantExpr::getFAdd(c1,c2);
      } break;
      case Instruction::Sub: {
        return ConstantExpr::getSub(c1,c2);
      } break;
      case Instruction::FSub: {
        return ConstantExpr::getFSub(c1,c2);
      } break;
      case Instruction::Mul: {
        return ConstantExpr::getMul(c1,c2);
      } break;
      case Instruction::FMul: {
        return ConstantExpr::getFMul(c1,c2);
      } break;
      case Instruction::UDiv: {
        return ConstantExpr::getUDiv(c1,c2);
      } break;
      case Instruction::SDiv: {
        return ConstantExpr::getSDiv(c1,c2);
      } break;
      case Instruction::FDiv: {
        return ConstantExpr::getFDiv(c1,c2);
      } break;
      
      case Instruction::ICmp: {
        CmpInst *cmpInst = dyn_cast<CmpInst>(&*I);
        return ConstantExpr::getICmp(cmpInst->getPredicate(),c1,c2);
      } break;
    }
    return NULL;
  }
  void handleFuncCall(const_Edge * in,Instruction * I){
    //anything returned is bottom
    const_Fact * newFact = new const_Fact();
    in->setValue(I,newFact);
    in->isTop = false;
    //anything passed in by reference is now bottom
    CallInst * c = dyn_cast<CallInst>(I);
    for(unsigned int i = 0; i < c->getNumArgOperands(); i++) {
      Value * op = c->getArgOperand(i);
      //all info for any operands are now considered useless
      in->kill(op);
    }
  }
  const_Edge * mapTerminator(const_Edge * in, const_Node *node) {
    //Future idea : add support for terminator instructions (this might take too much work)
    free(node->e);
    node->e = new const_Edge(in);
    return in;
  }
};


class const_latticeObj
{
  public:
    const_latticeObj(){
      //errs() << "Lattice Created" << "\n";
    }
    //checks for equality between edges
    bool comparator(const_Edge * F1,const_Edge * F2){
      if(F1->isTop != F2->isTop){
        return false;
      }
      //1. check that size is the same for # of entries
      if(F1->data->size() != F2->data->size()){
        return false;
      }
      //2. iterate over each value and find the corresponding fact.
      map<Value *,const_Fact *>::iterator it;
      for(it = F1->data->begin(); it != F1->data->end(); it++) {
        //2a. check that they both have the same entry
        if(F2->data->count(it->first) != 1) {
          return false;
        }
        const_Fact * entry = F2->data->find(it->first)->second;

        //2b. check that the entry fact is equivalent
        const_Fact * t = it->second;
        if(t->possibleValues != entry->possibleValues && t->isBottom != entry->isBottom) {
          return false;
        }

      }
      return true;
    }
    //merge vector of edges
    const_Edge * merge(vector<const_Edge *> edges){
      //create resulting edge
      const_Edge * result = new const_Edge();
      //for each edge:
      for(unsigned int i=0;i<edges.size();i++){
        //merge all the facts of the incoming edges
        map<Value *,const_Fact *>::iterator it;
        for(it = edges[i]->data->begin(); it != edges[i]->data->end(); it++) {
          result->addFact(it->first, it->second);
          
        }
      }
      result->isTop = result->data->size() == 0;
      return result;
    }
};


class const_workListObj
{
  private:
    
    const_latticeObj* optimizationLattice;
    const_FlowFunctions* FF;
    
  public:
    map<int, const_BBNode*> bbMap;
    //initialization: setup worklist components
    const_workListObj(int type){
      //errs() << "Const Worklist Created" << "\n";
      optimizationLattice = new const_latticeObj();
      FF = new const_FlowFunctions();
    }

    const_BBNode* lookupBB(llvm::StringRef tempID){
      int id = atoi(tempID.str().c_str());
      return bbMap[id];
    }
    
    //connect references to BB and instructions
    //initialize all edges to bottom
    void init(llvm::Module &M){
      //iterate over all functions in module
      int bbID = 0;
			for (Module::iterator F = M.begin(), Fe = M.end(); F != Fe; ++F){
				//iterate over all blocks in a function (that aren't the instrumented
				for (Function::iterator B = F->begin(), Be = F->end(); B != Be; ++B) {
          //create a const_BBNode for each basic block
          B->setName(llvm::Twine(bbID));
          //create a const_BBNode for each block
          bbMap[bbID] = new const_BBNode(bbID,B);
          bbID++;
				}
        //the start of every function is set to top
        Function::iterator first = F->begin();
        if(first != F->end()) {
          lookupBB(first->getName())->incoming->isTop = true;
        }
			}

      //setup the const_BBNodes with their outgoing edges now that they have ids
      for(map<int, const_BBNode*>::iterator it = bbMap.begin(); it != bbMap.end(); it++) {
        it->second->setup();
      }
    }
    
    vector<const_Edge *> getIncoming_Edges(const_BBNode* N) {
      vector<const_Edge *> incoming_Edges;
      int id = atoi(N->originalBB->getName().str().c_str());
      //3. get incoming edges from predecessors
      for (pred_iterator PI = pred_begin(N->originalBB), E = pred_end(N->originalBB); PI != E; ++PI) {
        //add to a vector of edges
        BasicBlock *Pred = *PI;
        incoming_Edges.push_back(lookupBB(Pred->getName())->outgoing[id]);
      }
      //if the block is the first in the function, then add that as well
      if(N->incoming->isTop == true){
        incoming_Edges.push_back(N->incoming);
      }
      return incoming_Edges;
    }

    //run does the actual worklist algorithm...
    void run(){
      //1. create a vector of const_BBNodes
      vector<const_BBNode *> worklist;
      //2. enqueue each const_BBNode
      map<int, const_BBNode*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        worklist.push_back(it->second);
      }
      
      while(worklist.size() > 0) {
        //get any const_BBNode
        const_BBNode *current_Node = worklist.back();
        worklist.pop_back();
        
        //3. get incoming edges from predecessors
        vector<const_Edge *> incoming_Edges = getIncoming_Edges(current_Node);
        //merge these incoming edges to form the node input
        const_Edge * in = optimizationLattice->merge(incoming_Edges);
        //set incoming edge to the new merged one
        free(current_Node->incoming);
        current_Node->incoming = in;
        const_Edge * inCopy = new const_Edge(in);
        
        //4. run flow function on const_BBNode
        map<int, const_Edge *> out = FF->m(inCopy,current_Node);
        
        //foreach output edge...
        map<int, const_Edge *>::iterator it;
        for(it = out.begin(); it != out.end(); it++) {
          int id = it->first;
          const_Edge * edgeOut = it->second;
          //5. check if the output edge has changed:
          if(optimizationLattice->comparator(edgeOut,current_Node->outgoing[id]) == false){
            free(current_Node->outgoing[id]);
            current_Node->outgoing[id] = edgeOut;
            const_BBNode * N = bbMap[id];
            vector<const_Edge *> succ_Edges = getIncoming_Edges(N);
            const_Edge * result = optimizationLattice->merge(succ_Edges);
            //6. if change: update the incoming edge and enqueue successors
            if(optimizationLattice->comparator(result,N->incoming) == false) {
              free(N->incoming);
              N->incoming = result;
              worklist.push_back(N);
            }
          }
        }
      }
    }
 
//PRINT FUNCTIONS (just for debugging)
    void printBottom(){
      //print out the relations for each block
      map<int, const_BBNode*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        const_BBNode * cBB = it->second;
        //print the BB id
        errs() << "-=BLOCK " << cBB->originalBB->getName() << " =-\n";
        //print output foreach successor
        map<int, const_Edge *>::iterator out;
        for(out = cBB->outgoing.begin(); out != cBB->outgoing.end(); out++) {
          errs() << "--Sucessor:  #" << out->first << "\n";
          const_Edge * cconst_Edge = out->second;
          //print the input facts
          errs() << "\tEdge : { ";
          if(cconst_Edge->isTop){
            errs() << "Top }\n";
          } else {
            map<Value *,const_Fact *> * info = cconst_Edge->data;
            map<Value *,const_Fact *>::iterator inf;
            for(inf = info->begin(); inf != info->end(); inf++) {
              printEntry(inf->first,inf->second);
            }
            errs() << "\n";
          }
        }
      }
    }
    void printTop(){
      //print out the relations for each block
      map<int, const_BBNode*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        const_BBNode * cBB = it->second;
        //print the BB id
        errs() << "-=BLOCK " << cBB->originalBB->getName() << " =-\n";
        //print the input facts
        errs() << "Edge : { ";
        if(cBB->incoming->isTop){
          errs() << "Top }\n";
        } else {
          map<Value *,const_Fact *> * info = cBB->incoming->data;
          map<Value *,const_Fact *>::iterator it;
          for(it = info->begin(); it != info->end(); it++) {
            printEntry(it->first,it->second);
          }
          errs() << "\n";
        }
      }
    }

    //print out the value and the info it is pointing to...
    void printEntry(Value * v, const_Fact * f){
      errs() << "{ " << *v << ": {";
      printconst_Fact(f);
      errs() << "}" << " },\n";
    }

    void printconst_Fact(const_Fact * f){
      if(f->isBottom == true){
        errs() << "*";
      } else {
        //print the set!
        set<Value *>::iterator it;
        for(it = f->possibleValues.begin(); it != f->possibleValues.end(); it++) {
          Value * single = *it;
          errs() << *single << ", ";
        }
      }
    }
};
