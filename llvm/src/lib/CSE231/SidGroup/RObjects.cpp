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
#include <limits>

using namespace std;
using namespace llvm;

/*
  TODO : need to define behavior of analysis
    Current Idea:
      a variable will generally get stored with its starting value and then increment/decrement towards a limit.  This analysis will record the starting value if it is a simple constant, and then record the icmp associated with it.  If a complex (non-constant) store is detected, we just skip it.  It is ok for this analysis to not be perfect, since it is just a simple warning system- ie, it is safer to not warn the programmer than to warn for an out-of-bounds that may not exist.
*/

class Range
{
  public:
  Value * min;
  Value * max;
  bool minIncl;
  bool maxIncl;
  Range() {
    minIncl = true;
    maxIncl = true;
  }
  bool operator==(Range &cR){
    return (min==cR.min) && (max==cR.max) && (minIncl==cR.minIncl) && (maxIncl==cR.maxIncl);
  }
};

class r_Fact
{
  public:
  bool isBottom; //everything defaults to bottom until a value is added
  bool isTop;    //ranges cover all numbers
  vector<Range> ranges;
  Range * latest;

  r_Fact() {
    isBottom = true;
  }
  r_Fact(r_Fact * parent) {
    //check case where the values aren't defined yet
    if(parent != NULL){
      isBottom = parent->isBottom;
      ranges = parent->ranges;
    } else {
      isBottom = true;
    }
  }
  
  void mergeRanges(){
    //for each range, merge the min and max values to create a single range.
    Range result;
    int min = numeric_limits<int>::max();
    int max = numeric_limits<int>::min();
    bool maxI = false;
    bool minI = false;
    //for each fact-
    for(unsigned int i=0;i<ranges.size();i++) {
      //if the min is smaller than current min, then set that as the min.
      int tMin = (int)dyn_cast<ConstantInt>(ranges[i].min)->getSExtValue();
      int tMax = (int)dyn_cast<ConstantInt>(ranges[i].max)->getSExtValue();
      if(min > tMin){
        min = tMin;
        minI = ranges[i].minIncl;
      }
      if(max < tMax){
        max = tMax;
        maxI = ranges[i].maxIncl;
      }
    }
    //remove all ranges and create a new one with the correct results
    result.min = ConstantInt::get(ranges[0].min->getType(), min);
    result.max = ConstantInt::get(ranges[0].min->getType(), max);
    result.minIncl = minI;
    result.maxIncl = maxI;

    ranges.clear();
    ranges.push_back(result);
  }
};

class Edge
{
  public:
  bool isTop;
  bool isBottom;
  map<Value *,r_Fact *> * data;
  //holds comparisons that have yet to be evaluated to true or false.
  vector<Value *> * pending;

  Edge(){
    isTop = false;
    isBottom = true;
    data = new map<Value *,r_Fact *>();
    //pending = new vector<Value *>;
  }
  //copy constructor for edge
  Edge(Edge * parent){
    isTop = parent->isTop;
    isBottom = parent->isBottom;
    data = new map<Value *,r_Fact *>();
    map<Value *,r_Fact *>::iterator it;
    for(it = parent->data->begin(); it != parent->data->end(); it++) {
      addr_Fact(it->first,it->second);
    }
  }
  void addr_Fact(Value * key, r_Fact * f){ //can add multiple values, preserves bottom state
    if(data->count(key) == 0) {
      data->insert(pair<Value *,r_Fact *>(key,new r_Fact(f)));
    } else {
      r_Fact * ef = data->find(key)->second;
      ef->isBottom = (ef->isBottom && f->isBottom);
      ef->isTop = (ef->isBottom || f->isBottom);
      ef->ranges.insert(ef->ranges.end(),f->ranges.begin(),f->ranges.end());
      ef->mergeRanges();
    }
  }
  //used to keep track of assignment-
  void addValue(Value * key, Value * possibleValue) {
    r_Fact * f;
    Range temp; //create a new range to hold the value
    temp.min = possibleValue;
    temp.max = possibleValue;
    if(data->count(key) == 0) { //check if key w/ r_Fact exists
      f = new r_Fact();
      f->isBottom = false;
      //add new r_Fact
      data->insert(pair<Value *,r_Fact *>(key,f));
    } else {
      f = data->find(key)->second;
    }
    //insert a range into the r_Fact
    f->ranges.push_back(temp);
    f->latest = &temp;
    f->mergeRanges();
    isTop = false;
  }

  void addRange(CmpInst * I, CmpInst::Predicate P) {
    //create a range and set the min and max, based on I and P
    //only possible if first operand is a variable and the second is a constant
    Value * c = I->getOperand(1);
    if(isa<Constant>(c)){
      Value * key = I->getOperand(0);
      //add it as if it was an assignment
      addValue(key,c);
      r_Fact *f = &*data->find(key)->second;
      //grab the new range
      Range * newRange = &f->ranges.back();
      //now, pivot around that range depending on the predicate
      switch(P){
        case CmpInst::ICMP_EQ : {
          //nothing to be done for this case, the r_Fact is already set to this
        } break;
        case CmpInst::ICMP_NE : {
          //just set to bottom in this case
          newRange->max = ConstantInt::get(c->getType(), numeric_limits<int>::max());
          newRange->min = ConstantInt::get(c->getType(), numeric_limits<int>::min());
          newRange->maxIncl = false;
          newRange->minIncl = false;
        } break;
        case CmpInst::ICMP_SGT :
        case CmpInst::ICMP_UGT : {
          newRange->max = ConstantInt::get(c->getType(), numeric_limits<int>::max());
          newRange->maxIncl = false;
          newRange->minIncl = false;
        } break;
        case CmpInst::ICMP_SGE :
        case CmpInst::ICMP_UGE : {
          newRange->max = ConstantInt::get(c->getType(), numeric_limits<int>::max());
          newRange->maxIncl = false;
          newRange->minIncl = true;
        } break;
        case CmpInst::ICMP_SLT :
        case CmpInst::ICMP_ULT : {
          newRange->min = ConstantInt::get(c->getType(), numeric_limits<int>::min());
          newRange->maxIncl = false;
          newRange->minIncl = false;
        } break;
        case CmpInst::ICMP_SLE :
        case CmpInst::ICMP_ULE : {
          newRange->min = ConstantInt::get(c->getType(), numeric_limits<int>::min());
          newRange->maxIncl = true;
          newRange->minIncl = false;
        } break;
        default : {
          //errs() << "ICMP_DEF Case\n";
        } break;
      }
      f->mergeRanges();
    } else {
      //errs() << *c << "is not a constant!\n";
    }
  }
  
  void kill(Value * key) {
    if(data->count(key) == 1) { //check that key exists
      data->erase(data->find(key));
    }
  }
};

class r_Node
{
  public:
  Instruction * oI;
  Edge * data;
  
  r_Node(Instruction * I){
    data = new Edge();
    oI = I;
  }
};

class r_BBNode
{
  public:
  int bID;
  BasicBlock * originalBB;
  Edge * incoming;
  //can have multiple outgoing edges
  map<int, Edge *> outgoing;
  vector<r_Node *> nodes;
  
  r_BBNode(int ibID, BasicBlock * BB){
    bID = ibID;
    originalBB = BB;
    incoming = new Edge();
    //loop over each instruction in orginalBB and create a r_Node to point to them
    BasicBlock::iterator I = originalBB->begin(), Ie = originalBB->end();
    for (; I != Ie; ++I) {
      nodes.push_back(new r_Node(I));
    }
  }
  //use the names of the successor BBs to add the outgoing edges
  void setup() {
    for (succ_iterator SI = succ_begin(originalBB), E = succ_end(originalBB); SI != E; ++SI) {
      BasicBlock *Succ = *SI;
      int id = atoi(Succ->getName().str().c_str());
      outgoing[id] = new Edge();
    }
  }
};

class r_FlowFunctions
{
  public:
  r_FlowFunctions(){
    //errs() << "Flow Functions Created" << "\n";
  }
  
  map<int, Edge *> m(Edge * in, r_BBNode * N){
    map<int, Edge *> result;
    //go over each instruction in the BB and apply the flow function!!!
    for(unsigned int i=0;i<N->nodes.size() - 1;i++){
      in = mapInstruction(in, N->nodes[i]);
    }
    in->isTop = false;
    //then use the terminating instructions to make an edge for each successor
    int index = 0;
    for (succ_iterator SI = succ_begin(N->originalBB), E = succ_end(N->originalBB); SI != E; ++SI) {
      BasicBlock *Succ = *SI;
      int id = atoi(Succ->getName().str().c_str());
      result[id] = mapTerminator(new Edge(in), index, N->originalBB->getTerminator());
      index++;
    }
    return result;
  }

  //do flow function calls here
  Edge * mapInstruction(Edge *in, r_Node *node) {
    if(node->oI->getOpcode() == Instruction::ICmp) {
      in->kill(node->oI->getOperand(0));
      //add this r_Fact to a list of "pending" for the edge.  On actual branching, we will have to add the range, or invert the range and add that.
      //in->pending->push_back(node->oI);
      in->isTop = false;
    }
    //load case: save this assignment to r_Fact
    if(node->oI->getOpcode() == Instruction::Store) {
      //kill current ranges for this r_Fact since it is being overwritten
      in->kill(node->oI->getOperand(1));
      //TODO : should we support simple addition an subtraction?
      //only do save if the store operand is a simple constant
      if(isa<Constant>(node->oI->getOperand(0))) {
        in->addValue(node->oI->getOperand(1),node->oI->getOperand(0));
      }
      in->isTop = false;
    }
    free(node->data);
    node->data = new Edge(in);
    return in;
  }
  //TODO : need to consider switch terminators?!
  Edge * mapTerminator(Edge * in, int sIndex, Instruction * I) {
    //if the terminating instruction is a conditional branch, then evaluate it!
    if(I->getOpcode() == Instruction::Br) {
      if(I->getNumOperands() == 3) {
        CmpInst * conditional = dyn_cast<CmpInst>(I->getOperand(0));
        CmpInst::Predicate predi = conditional->getPredicate();
        //if it is the false case, then get the inverse predicate
        if(sIndex == 1){
          predi = CmpInst::getInversePredicate(predi);
        }
        //Add the range:
        in->addRange(conditional,predi);
      }
    }
    return in;
  }
};


class r_latticeObj
{
  public:
    r_latticeObj(){
      //errs() << "Lattice Created" << "\n";
    }
    //checks for equality between edges
    bool comparator(Edge * F1,Edge * F2){
      if(F1->isTop != F2->isTop){
        return false;
      }
      //1. check that size is the same
      if(F1->data->size() != F2->data->size()){
        return false;
      }
      //2. iterate over each value and find the corresponding r_Fact.
      map<Value *,r_Fact *>::iterator it;
      for(it = F1->data->begin(); it != F1->data->end(); it++) {
        //2a. check that they both have the same entry
        if(F2->data->count(it->first) != 1) {
          return false;
        }
        r_Fact * entry1 = it->second;
        r_Fact * entry2 = F2->data->find(it->first)->second;
        //check ranges size
        if(entry1->ranges.size() != entry2->ranges.size()){
          return false;
        }
        vector<Range>::iterator itr1;
        for(itr1 = entry1->ranges.begin(); itr1 != entry1->ranges.end(); itr1++) {
          vector<Range>::iterator itr2;
          bool isFound = false;
          for(itr2 = entry2->ranges.begin(); itr2 != entry2->ranges.end(); itr2++) {
            if(*itr1 == *itr2){
              isFound = true;
            }
          }
          if(isFound == false){
            return false;
          }
        }
      }
      return true;
    }
    //merge vector of edges
    Edge * merge(vector<Edge *> edges){
      //create resulting edge
      Edge * result = new Edge();
      //for each edge:
      for(unsigned int i=0;i<edges.size();i++){
        //merge all the r_Facts of the incoming edges
        map<Value *,r_Fact *>::iterator it;
        for(it = edges[i]->data->begin(); it != edges[i]->data->end(); it++) {
          //TODO : do merging instead of adding
          result->addr_Fact(it->first, it->second);
        }
      }
      result->isTop = result->data->size() == 0;
      return result;
    }
};

class r_workListObj
{
  private:
    
    r_latticeObj* optimizationLattice;
    r_FlowFunctions* FF;
    
  public:
    std::map<int, r_BBNode*> bbMap;
    //initialization: setup worklist components
    r_workListObj(int type){
      //llvm::errs() << "Range Analysis Worklist Created" << "\n";
      optimizationLattice = new r_latticeObj();
      FF = new r_FlowFunctions();
    }

    r_BBNode* lookupBB(llvm::StringRef tempID){
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
          //create a r_BBNode for each basic block
          B->setName(llvm::Twine(bbID));
          //create a r_BBNode for each block
          bbMap[bbID] = new r_BBNode(bbID,B);
          bbID++;
				}
        //the start of every function is set to top
        Function::iterator first = F->begin();
        if(first != F->end()) {
          lookupBB(first->getName())->incoming->isTop = true;
        }
			}

      //setup the r_BBNodes with their outgoing edges now that they have ids
      for(map<int, r_BBNode*>::iterator it = bbMap.begin(); it != bbMap.end(); it++){
        it->second->setup();
      }
    }
    
    vector<Edge *> getIncomingEdges(r_BBNode* N){
      vector<Edge *> incomingEdges;
      int id = atoi(N->originalBB->getName().str().c_str());
      //3. get incoming edges from predecessors
      for (pred_iterator PI = pred_begin(N->originalBB), E = pred_end(N->originalBB); PI != E; ++PI) {
        //add to a vector of edges
        BasicBlock *Pred = *PI;
        incomingEdges.push_back(lookupBB(Pred->getName())->outgoing[id]);
      }
      //if the block is the first in the function, then add that as well
      if(N->incoming->isTop == true){
        incomingEdges.push_back(N->incoming);
      }
      return incomingEdges;
    }

    //run does the actual worklist algorithm...
    void run(){
      //errs() << "Running worklist algorithm..." << "\n";
      
      //1. create a vector of r_BBNodes
      vector<r_BBNode *> worklist;
      //2. enqueue each r_BBNode
      map<int, r_BBNode*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        worklist.push_back(it->second);
      }
      
      while(worklist.size() > 0) {
        //get any r_BBNode
        r_BBNode *currentNode = worklist.back();
        worklist.pop_back();
        
        //3. get incoming edges from predecessors
        vector<Edge *> incomingEdges = getIncomingEdges(currentNode);
        //merge these incoming edges to form the node input
        Edge * in = optimizationLattice->merge(incomingEdges);
        //set incoming edge to the new merged one
        free(currentNode->incoming);
        currentNode->incoming = in;
        Edge * inCopy = new Edge(in);
        
        //4. run flow function on r_BBNode
        map<int, Edge *> out = FF->m(inCopy,currentNode);
        
        //foreach output edge...
        map<int, Edge *>::iterator it;
        for(it = out.begin(); it != out.end(); it++) {
          int id = it->first;
          Edge * edgeOut = it->second;
          //5. check if the output edge has changed:
          if(optimizationLattice->comparator(edgeOut,currentNode->outgoing[id]) == false){
            free(currentNode->outgoing[id]);
            currentNode->outgoing[id] = edgeOut;
            r_BBNode * N = bbMap[id];
            vector<Edge *> succEdges = getIncomingEdges(N);
            Edge * result = optimizationLattice->merge(succEdges);
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
//TODO : redo these... 
//PRINT FUNCTIONS (just for debugging)
    void printBottom(){}
    void printTop(){
      //print out the relations for each block
      map<int, r_BBNode*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        r_BBNode * cBB = it->second;
        //print the BB id
        errs() << "-=BLOCK " << cBB->originalBB->getName() << " =-\n";
        //print the input r_Facts
        errs() << "Edge : { ";
        if(cBB->incoming->isTop){
          errs() << "Top }\n";
        } else {
          map<Value *,r_Fact *> * info = cBB->incoming->data;
          map<Value *,r_Fact *>::iterator it;
          for(it = info->begin(); it != info->end(); it++) {
            printEntry(it->first,it->second);
          }
          errs() << "\n";
        }
      }
    }

    //print out the value and the info it is pointing to...
    void printEntry(Value * v, r_Fact * f){
      errs() << "{ " << *v << ": {";
      printr_Fact(f);
      errs() << "}" << " },\n";
    }

    void printr_Fact(r_Fact * f){
      if(f->isBottom == true){
        errs() << "*";
      } else {
        //print the set!
        vector<Range>::iterator it;
        for(it = f->ranges.begin(); it != f->ranges.end(); it++) {
          Range single = *it;
          errs() << *single.min << "-" << *single.max << ", ";
        }
      }
    }
};
