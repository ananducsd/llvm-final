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
#include <algorithm>
#include "MetaLattice.cpp"

using namespace std;
using namespace llvm;
using namespace metalattice;


namespace avlexp{

class EdgeFact
{
  public:
  bool isTop;
  bool isBottom;
  set<Instruction *> aExpr;

  EdgeFact(){
    isTop = false;
    isBottom = true;
  }
  //copy constructor for edge
  EdgeFact(EdgeFact * parent){
    isTop = parent->isTop;
    isBottom = parent->isBottom;
    aExpr.insert(parent->aExpr.begin(),parent->aExpr.end());
  }

  void addValue(Instruction * I) {
    //foreach available expression, see if this is equivalent to any existing.
    set<Instruction *>::iterator it;
    for(it = aExpr.begin(); it != aExpr.end(); it++) {
      Instruction * temp = *it;
      //if it is, then skip it.
      if(I->isIdenticalTo(temp)){
        return;
      }
    }
    //if it isn't, then store it.
    aExpr.insert(I);
  }
  void removeValue(Instruction * key) {
    if(aExpr.count(key) == 1) { //check that key exists
      aExpr.erase(aExpr.find(key));
    }
  }
};



class FlowFunctions
{
  public:
  FlowFunctions(){
    //errs() << "Flow Functions Created" << "\n";
  }

  map<int, EdgeFact *> runFlowOnBlock(EdgeFact * in, BBNode<EdgeFact> * N){
    map<int, EdgeFact *> result;
    //go over each instruction in the BB and apply the flow function!!!
    for(unsigned int i=0;i<N->nodes.size() - 1;i++){
      in = runFlowFn(in, N->nodes[i]);
    }
    in->isBottom = false;

    //use the terminating instructions to make an edge for each successor
    for (succ_iterator SI = succ_begin(N->originalBB), E = succ_end(N->originalBB); SI != E; ++SI) {
      BasicBlock *Succ = *SI;
      int id = atoi(Succ->getName().str().c_str());
      result[id] = mapTerminator(new EdgeFact(in));
    }
    return result;
  }
  //do flow function calls here
  EdgeFact * runFlowFn(EdgeFact * in, Node<EdgeFact> *node) {
    //free old input to the node and save the new one
    free(node->e);
    node->e = new EdgeFact(in); //makes a copy
    //treat stores differently: they destroy available expressions
    if(node->I->getOpcode() == Instruction::Store){
      in->removeValue((Instruction *)node->I->getOperand(1));
      in->addValue(node->I);
    } else {
      in->addValue(node->I);
    }
    return in;
  }

  EdgeFact * mapTerminator(EdgeFact * in) {
    //Future Idea : add support for terminator instructions
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
      if(F1->aExpr.size() != F2->aExpr.size()){
        return false;
      }
      //2. iterate over each value and find the corresponding fact.
      set<Instruction *>::iterator it;
      for(it = F1->aExpr.begin(); it != F1->aExpr.end(); it++) {
        //2a. check that they both have the same entry
        Instruction * entry = *it;
        if(F2->aExpr.count(entry) != 1) {
          return false;
        }
      }
      return true;
    }
    //merge vector of edges.  this is an intersection of sets
    EdgeFact * merge(vector<EdgeFact *> edges){
      //create resulting edge
      EdgeFact * result;
      if(edges.size() > 1) {
        result = new EdgeFact();
        result->isBottom = false;
        set<Instruction *> * previous = &edges[0]->aExpr;
        for(unsigned int i=1;i<edges.size();i++){
          set<Instruction *> intersect;
          set_intersection(previous->begin(),previous->end(),edges[i]->aExpr.begin(),edges[i]->aExpr.end(),inserter(intersect,intersect.begin()));
          result->aExpr = intersect;
          previous = &edges[i]->aExpr;
        }
      } else {
        if(edges.size() == 1){
          result = new EdgeFact(edges[0]);
        } else {
          result = new EdgeFact();
        }
      }
      return result;
    }
};


class PrintUtil {
//PRINT FUNCTIONS (just for debugging)
  public:
    map<int, BBNode<EdgeFact>*> bbMap;

    void printBottom(){
      errs() << "\n\n\nOutgoing edges:\n";
      //print out the relations for each block
      map<int, BBNode<EdgeFact>*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        BBNode<EdgeFact> * cBB = it->second;
        //print the BB id
        errs() << "-=BLOCK " << cBB->originalBB->getName() << " =-\n";
        //print output foreach successor
        map<int, EdgeFact *>::iterator out;
        for(out = cBB->outgoing.begin(); out != cBB->outgoing.end(); out++) {
          errs() << "--Sucessor:  #" << out->first << "\n";
          EdgeFact * cEdge = out->second;
          //print the input facts
          errs() << "\tEdge : { ";
          if(cEdge->isBottom == true){
            errs() << "Top }\n";
          } else {
            set<Instruction *>::iterator itr;
            for(itr = cEdge->aExpr.begin(); itr != cEdge->aExpr.end(); itr++) {
              printEntry(*itr);
            }
            errs() << " } \n";
          }
        }
      }
    }
    void printTop(){
      errs() << "\n\n\nIncoming edges:\n";
      //print out the relations for each block
      map<int, BBNode<EdgeFact>*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        BBNode<EdgeFact> * cBB = it->second;
        EdgeFact * in = cBB->incoming;
        //print the BB id
        errs() << "-=BLOCK " << cBB->originalBB->getName() << " =-\n";
        //print the input facts
        errs() << "EdgeFact : { ";
        if(in->isBottom == true){
          errs() << "* } \n";
        } else {
          set<Instruction *>::iterator itr;
          for(itr = in->aExpr.begin(); itr != in->aExpr.end(); itr++) {
            printEntry(*itr);
          }
          errs() << " } \n";
        }
      }
    }

    //print out the value and the info it is pointing to...
    void printEntry(Instruction * v){
      errs() << "{" << *v << "}";
      errs() << ",\n";
    }
};

}
