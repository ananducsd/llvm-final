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
#include "MetaLattice.cpp"

using namespace std;
using namespace llvm;
using namespace metalattice;


namespace range{

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

class Fact
{
  public:
  bool isBottom; //everything defaults to bottom until a value is added
  bool isTop;    //ranges cover all numbers
  vector<Range> ranges;
  Range * latest;

  Fact() {
    isBottom = true;
  }
  Fact(Fact * parent) {
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

class EdgeFact
{
  public:
  bool isTop;
  bool isBottom;
  map<Value *,Fact *> * data;
  //holds comparisons that have yet to be evaluated to true or false.
  vector<Value *> * pending;

  EdgeFact(){
    isTop = false;
    isBottom = true;
    data = new map<Value *,Fact *>();
    //pending = new vector<Value *>;
  }
  //copy constructor for edge
  EdgeFact(EdgeFact * parent){
    isTop = parent->isTop;
    isBottom = parent->isBottom;
    data = new map<Value *,Fact *>();
    map<Value *,Fact *>::iterator it;
    for(it = parent->data->begin(); it != parent->data->end(); it++) {
      addr_Fact(it->first,it->second);
    }
  }
  void addr_Fact(Value * key, Fact * f){ //can add multiple values, preserves bottom state
    if(data->count(key) == 0) {
      data->insert(pair<Value *,Fact *>(key,new Fact(f)));
    } else {
      Fact * ef = data->find(key)->second;
      ef->isBottom = (ef->isBottom && f->isBottom);
      ef->isTop = (ef->isBottom || f->isBottom);
      ef->ranges.insert(ef->ranges.end(),f->ranges.begin(),f->ranges.end());
      ef->mergeRanges();
    }
  }
  //used to keep track of assignment-
  void addValue(Value * key, Value * possibleValue) {
    Fact * f;
    Range temp; //create a new range to hold the value
    temp.min = possibleValue;
    temp.max = possibleValue;
    if(data->count(key) == 0) { //check if key w/ Fact exists
      f = new Fact();
      f->isBottom = false;
      //add new Fact
      data->insert(pair<Value *,Fact *>(key,f));
    } else {
      f = data->find(key)->second;
    }
    //insert a range into the Fact
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
      Fact *f = &*data->find(key)->second;
      //grab the new range
      Range * newRange = &f->ranges.back();
      //now, pivot around that range depending on the predicate
      switch(P){
        case CmpInst::ICMP_EQ : {
          //nothing to be done for this case, the Fact is already set to this
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

class FlowFunctions
{
  public:
  FlowFunctions(){
  }

  map<int, EdgeFact *> runFlowOnBlock(EdgeFact * in, BBNode<EdgeFact> * N){
    map<int, EdgeFact *> result;
    //go over each instruction in the BB and apply the flow function!!!
    for(unsigned int i=0;i<N->nodes.size() - 1;i++){
      in = runFlowFn(in, N->nodes[i]);
    }
    in->isTop = false;
    //then use the terminating instructions to make an edge for each successor
    int index = 0;
    for (succ_iterator SI = succ_begin(N->originalBB), E = succ_end(N->originalBB); SI != E; ++SI) {
      BasicBlock *Succ = *SI;
      int id = atoi(Succ->getName().str().c_str());
      result[id] = mapTerminator(new EdgeFact(in), index, N->originalBB->getTerminator());
      index++;
    }
    return result;
  }

  //do flow function calls here
  EdgeFact * runFlowFn(EdgeFact *in, Node<EdgeFact> *node) {
    if(node->I->getOpcode() == Instruction::ICmp) {
      in->kill(node->I->getOperand(0));
      //add this Fact to a list of "pending" for the edge.  On actual branching, we will have to add the range, or invert the range and add that.
      //in->pending->push_back(node->I);
      in->isTop = false;
    }
    //load case: save this assignment to Fact
    if(node->I->getOpcode() == Instruction::Store) {
      //kill current ranges for this Fact since it is being overwritten
      in->kill(node->I->getOperand(1));
      //TODO : should we support simple addition an subtraction?
      //only do save if the store operand is a simple constant
      if(isa<Constant>(node->I->getOperand(0))) {
        in->addValue(node->I->getOperand(1),node->I->getOperand(0));
      }
      in->isTop = false;
    }
    free(node->e);
    node->e = new EdgeFact(in);
    return in;
  }
  //TODO : need to consider switch terminators?!
  EdgeFact * mapTerminator(EdgeFact * in, int sIndex, Instruction * I) {
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


class Lattice
{
  public:
    Lattice(){
      //errs() << "Lattice Created" << "\n";
    }
    //checks for equality between edges
    bool comparator(EdgeFact * F1,EdgeFact * F2){
      if(F1->isTop != F2->isTop){
        return false;
      }
      //1. check that size is the same
      if(F1->data->size() != F2->data->size()){
        return false;
      }
      //2. iterate over each value and find the corresponding Fact.
      map<Value *,Fact *>::iterator it;
      for(it = F1->data->begin(); it != F1->data->end(); it++) {
        //2a. check that they both have the same entry
        if(F2->data->count(it->first) != 1) {
          return false;
        }
        Fact * entry1 = it->second;
        Fact * entry2 = F2->data->find(it->first)->second;
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
    EdgeFact * merge(vector<EdgeFact *> edges){
      //create resulting edge
      EdgeFact * result = new EdgeFact();
      //for each edge:
      for(unsigned int i=0;i<edges.size();i++){
        //merge all the r_Facts of the incoming edges
        map<Value *,Fact *>::iterator it;
        for(it = edges[i]->data->begin(); it != edges[i]->data->end(); it++) {
          //TODO : do merging instead of adding
          result->addr_Fact(it->first, it->second);
        }
      }
      result->isTop = result->data->size() == 0;
      return result;
    }
};

}

