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
#include "MetaLattice.cpp"

using namespace std;
using namespace llvm;
using namespace metalattice;

namespace constprop {

class Fact
{
  public:
  bool isBottom; //everything defaults to bottom until a value is added
  set<Value *> possibleValues;
  Fact() {
    isBottom = true;
  }
  Fact(set<Value *> startValues) {
    if(startValues.size() > 0) {
      possibleValues = startValues;
      isBottom = false;
      checkIfSuperconstant();
    } else {
      isBottom = true;
    }
  }
  Fact(Fact * parent) {
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

class Edge
{
  public:
  bool isTop;
  bool isBottom;

  // map from variable to constants  { (X, {3, 5}), (Y, {5} ) }
  map<Value *,Fact *> * data;

  Edge(){
    isTop = false;
    isBottom = true;
    data = new map<Value *,Fact *>();
  }
  //copy constructor for edge
  Edge(Edge * parent){
    isTop = parent->isTop;
    isBottom = parent->isBottom;
    data = new map<Value *,Fact *>();
    map<Value *,Fact *>::iterator it;
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

  Fact * lookup(Value * key){
    if(data->count(key) == 1) { //if key is instruction that has data already,
      return new Fact(data->find(key)->second); //then return that data
    } else {
      Fact * temp = new Fact(); //the key is not an instruction
      temp->isBottom = false;   //create a new fact to store the value
      temp->possibleValues.insert(key);
      temp->checkIfSuperconstant();
      return temp;
    }
  }
  void addFact(Value * key, Fact * f){ //can add multiple values, preserves bottom state
    if(data->count(key) == 0) {
      data->insert(pair<Value *,Fact *>(key,new Fact(f)));
    } else {
      Fact * ef = data->find(key)->second;
      ef->isBottom = (ef->isBottom || f->isBottom);
      if(!ef->isBottom){
        ef->possibleValues.insert(f->possibleValues.begin(),f->possibleValues.end());
      }
      ef->checkIfSuperconstant();
    }
  }
  void addValue(Value * key, Value * possibleValue) {
    Fact * f;
    if(data->count(key) == 0) { //check if key w/ fact exists
      f = new Fact();
      f->isBottom = false;
      data->insert(pair<Value *,Fact *>(key,f));
    } else {
      f = data->find(key)->second;
    }
    f->possibleValues.insert(possibleValue);
    f->checkIfSuperconstant();
    isTop = false;
  }
  void removeValue(Value * key, Value * possibleValue) {
    if(data->count(key) == 1) { //check that key exists
      Fact * f = data->find(key)->second;
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
  void setValue(Value * key, Fact * destPointer){
    if(data->count(key) == 1) { //free old fact and set it equal to destPointer
      //free(data->find(key)->second); //WARNING : this might be a memory leak, but is unsafe to use
      data->find(key)->second = destPointer;
    } else { //if key does not exist, just create a new pair
      data->insert(pair<Value *,Fact *>(key,destPointer));
    }
  }
};

class FlowFunctions
{
  public:
  FlowFunctions(){
    //errs() << "Flow Functions Created" << "\n";
  }
  // Given an incomming edge fact and basic block,  it runs  the flow function for entire block
  map<int, Edge *> m(Edge * in, BBNode<Edge> * N){
    // Map corresponding to terminator edge fact for all outgoing edges
    map<int, Edge *> result;

    //go over each instruction in the BB and apply the flow function!!!
    for(unsigned int i=0;i<N->nodes.size() - 1;i++){
      in = mapInstruction(in, N->nodes[i]);
    }
    in->isTop = false;

    //use the terminating instructions to make an edge for each successor
    for (succ_iterator SI = succ_begin(N->originalBB), E = succ_end(N->originalBB); SI != E; ++SI) {
      BasicBlock *Succ = *SI;
      int id = atoi(Succ->getName().str().c_str());
      result[id] = mapTerminator(new Edge(in),N->nodes.back());
    }
    return result;
  }

  //do flow function calls here
  Edge * mapInstruction(Edge *in, Node<Edge> *node) {
    switch(node->I->getOpcode()) {
      //X = *
      case Instruction::Alloca: { //done
        //due to SSA, we don't need to check this one
        Fact * newFact = new Fact(); //it defaults to bottom = true
        in->data->insert(pair<Value *,Fact *>(node->I,newFact));
        in->isTop = false;
      } break;
      //X* = Y
      case Instruction::Store: {
        //TODO : do a check that it isn't bottom in this one before killing it...
        if(in->data->count(node->I->getOperand(1))) {
          //Fact * t = in->data->find(node->I->getOperand(1))->second;
          //if(!t->isBottom){
            in->kill(node->I->getOperand(1)); //remove all references to X
          //}
        }
        Fact * toAdd = in->lookup(node->I->getOperand(0)); //generate the new fact for that operand
        in->addFact(node->I->getOperand(1), toAdd); //update that fact
      } break;
      //X = Y
      case Instruction::Load: { //working on!
        //due to SSA we can just add a fact where it has the value pointing to the Y value...
        Fact * newFact;
        if(in->data->count(node->I->getOperand(0)) == 1) {
          newFact = new Fact(in->data->find(node->I->getOperand(0))->second);
        } else {
          newFact = new Fact(); //case where there is no fact for Y
        }
        in->setValue(node->I,newFact);
        in->isTop = false;
      } break;
      default : {
        if(isa<CallInst>(node->I)){
          handleFuncCall(in,node->I);
          return in;
        }
        Fact * newFact;
        set<Value *> results;
        //NOTE: we currently only support 2 operand other instructions
        if(node->I->getNumOperands() == 2){
          if(in->hasUsefulData(node->I->getOperand(0)) && in->hasUsefulData(node->I->getOperand(1))){
            results = getAllResults(node->I,in);
            if(results.size() == 0){
              newFact = new Fact();
            } else {
              newFact = new Fact(results);
            }
          } else { //set value to bottom if an operand is unusable
            newFact = new Fact();
          }
        } else {
          newFact = new Fact();
        }
        //update edge with fact
        in->setValue(node->I,newFact);
        in->isTop = false;
        newFact->checkIfSuperconstant();
      } break;
    }
    //save incoming data to the node
    free(node->e);
    node->e = new Edge(in);
    return in;
  }

  set<Value *> getAllResults(Instruction * I, Edge * e) {
    set<Value *> results;
    set<Value *> bad;
    //step 1: cast the values as facts
    Fact * f1 = e->lookup(I->getOperand(0));
    Fact * f2 = e->lookup(I->getOperand(1));
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


  void handleFuncCall(Edge * in,Instruction * I){
    //anything returned is bottom
    Fact * newFact = new Fact();
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



  Edge * mapTerminator(Edge * in, Node<Edge> *node) {
    //Future idea : add support for terminator instructions (this might take too much work)
    free(node->e);
    node->e = new Edge(in);
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
    bool comparator(Edge * F1,Edge * F2){
      if(F1->isTop != F2->isTop){
        return false;
      }
      //1. check that size is the same for # of entries
      if(F1->data->size() != F2->data->size()){
        return false;
      }
      //2. iterate over each value and find the corresponding fact.
      map<Value *,Fact *>::iterator it;
      for(it = F1->data->begin(); it != F1->data->end(); it++) {
        //2a. check that they both have the same entry
        if(F2->data->count(it->first) != 1) {
          return false;
        }
        Fact * entry = F2->data->find(it->first)->second;

        //2b. check that the entry fact is equivalent
        Fact * t = it->second;
        if(t->possibleValues != entry->possibleValues && t->isBottom != entry->isBottom) {
          return false;
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
        //merge all the facts of the incoming edges
        map<Value *,Fact *>::iterator it;
        for(it = edges[i]->data->begin(); it != edges[i]->data->end(); it++) {
          result->addFact(it->first, it->second);

        }
      }
      result->isTop = result->data->size() == 0;
      return result;
    }
};

class PrintUtil {

public:
    map<int, BBNode<Edge> *> bbMap;

//PRINT FUNCTIONS (just for debugging)
    void printBottom(){
      //print out the relations for each block
      map<int, BBNode<Edge>*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        BBNode<Edge> * cBB = it->second;
        //print the BB id
        errs() << "-=BLOCK " << cBB->originalBB->getName() << " =-\n";
        //print output foreach successor
        map<int, Edge *>::iterator out;
        for(out = cBB->outgoing.begin(); out != cBB->outgoing.end(); out++) {
          errs() << "--Sucessor:  #" << out->first << "\n";
          Edge * cEdge = out->second;
          //print the input facts
          errs() << "\tEdge : { ";
          if(cEdge->isTop){
            errs() << "Top }\n";
          } else {
            map<Value *,Fact *> * info = cEdge->data;
            map<Value *,Fact *>::iterator inf;
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
      map<int, BBNode<Edge>*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        BBNode<Edge> * cBB = it->second;
        //print the BB id
        errs() << "-=BLOCK " << cBB->originalBB->getName() << " =-\n";
        //print the input facts
        errs() << "Edge : { ";
        if(cBB->incoming->isTop){
          errs() << "Top }\n";
        } else {
          map<Value *,Fact *> * info = cBB->incoming->data;
          map<Value *,Fact *>::iterator it;
          for(it = info->begin(); it != info->end(); it++) {
            printEntry(it->first,it->second);
          }
          errs() << "\n";
        }
      }
    }

    //print out the value and the info it is pointing to...
    void printEntry(Value * v, Fact * f){
      errs() << "{ " << *v << ": {";
      printFact(f);
      errs() << "}" << " },\n";
    }

    void printFact(Fact * f){
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

}
