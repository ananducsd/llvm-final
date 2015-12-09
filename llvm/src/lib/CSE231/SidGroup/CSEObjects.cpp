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

using namespace std;
using namespace llvm;

/*
  opt -load $LLVMLIB/CSE231.so -CSEPass < $BENCHMARKS/gcd/gcd.bc > temp.instrumented.bc

  Important info:
  http://llvm.org/docs/ProgrammersManual.html#replacing-an-instruction-with-another-value
*/

class CSE_Edge
{
  public:
  bool isTop;
  bool isBottom;
  set<Instruction *> aExpr;

  CSE_Edge(){
    isTop = false;
    isBottom = true;
  }
  //copy constructor for edge
  CSE_Edge(CSE_Edge * parent){
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

class CSE_Node
{
  public:
  Instruction * oI;
  CSE_Edge * e;
  
  CSE_Node(Instruction * I){
    e = new CSE_Edge();
    oI = I;
  }
};

class CSE_BBNode
{
  public:
  int bID;
  BasicBlock * originalBB;
  CSE_Edge * incoming;
  //can have multiple outgoing edges
  map<int, CSE_Edge *> outgoing;
  vector<CSE_Node *> nodes;
  
  CSE_BBNode(int ibID, BasicBlock * BB){
    bID = ibID;
    originalBB = BB;
    incoming = new CSE_Edge();
    //loop over each instruction in orginalBB and create a CSE_Node to point to them
    BasicBlock::iterator I = originalBB->begin(), Ie = originalBB->end();
    for (; I != Ie; ++I) {
      nodes.push_back(new CSE_Node(I));
    }
  }
  //use the names of the successor BBs to add the outgoing edges
  void setup() {
    for (succ_iterator SI = succ_begin(originalBB), E = succ_end(originalBB); SI != E; ++SI) {
      BasicBlock *Succ = *SI;
      int id = atoi(Succ->getName().str().c_str());
      outgoing[id] = new CSE_Edge();
    }
  }
};

class CSE_FlowFunctions
{
  public:
  CSE_FlowFunctions(){
    //errs() << "Flow Functions Created" << "\n";
  }
  
  map<int, CSE_Edge *> m(CSE_Edge * in, CSE_BBNode * N){
    map<int, CSE_Edge *> result;
    //go over each instruction in the BB and apply the flow function!!!
    for(unsigned int i=0;i<N->nodes.size() - 1;i++){
      in = mapInstruction(in, N->nodes[i]);
    }
    in->isBottom = false;
    
    //use the terminating instructions to make an edge for each successor
    for (succ_iterator SI = succ_begin(N->originalBB), E = succ_end(N->originalBB); SI != E; ++SI) {
      BasicBlock *Succ = *SI;
      int id = atoi(Succ->getName().str().c_str());
      result[id] = mapTerminator(new CSE_Edge(in));
    }
    return result;
  }
  //do flow function calls here
  CSE_Edge * mapInstruction(CSE_Edge * in, CSE_Node *node) {
    //free old input to the node and save the new one
    free(node->e);
    node->e = new CSE_Edge(in); //makes a copy
    //treat stores differently: they destroy available expressions
    if(node->oI->getOpcode() == Instruction::Store){
      in->removeValue((Instruction *)node->oI->getOperand(1));
      in->addValue(node->oI);
    } else {
      in->addValue(node->oI);
    }
    return in;
  }
  
  CSE_Edge * mapTerminator(CSE_Edge * in) {
    //Future Idea : add support for terminator instructions
    return in;
  }
};

class CSE_latticeObj
{
  public:
    CSE_latticeObj(){
      //errs() << "Lattice Created" << "\n";
    }
    //checks for equality between edges
    bool comparator(CSE_Edge * F1,CSE_Edge * F2){
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
    CSE_Edge * merge(vector<CSE_Edge *> edges){
      //create resulting edge
      CSE_Edge * result;
      if(edges.size() > 1) {
        result = new CSE_Edge();
        result->isBottom = false;
        set<Instruction *> * previous = &edges[0]->aExpr;
        for(unsigned int i=1;i<edges.size();i++){
          set<Instruction *> intersect;
          set_intersection(previous->begin(),previous->end(),edges[i]->aExpr.begin(),edges[i]->aExpr.end(),std::inserter(intersect,intersect.begin()));
          result->aExpr = intersect;
          previous = &edges[i]->aExpr;
        }
      } else {
        if(edges.size() == 1){
          result = new CSE_Edge(edges[0]);
        } else {
          result = new CSE_Edge();
        }
      }
      return result;
    }
};

class CSE_workListObj
{
  private:
    
    CSE_latticeObj* optimizationLattice;
    CSE_FlowFunctions* FF;
    
  public:
    std::map<int, CSE_BBNode*> bbMap;
    //initialization: setup worklist components
    CSE_workListObj(int type){
      optimizationLattice = new CSE_latticeObj();
      FF = new CSE_FlowFunctions();
    }

    CSE_BBNode* lookupBB(llvm::StringRef tempID){
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
          //create a CSE_BBNode for each basic block
          B->setName(llvm::Twine(bbID));
          //create a CSE_BBNode for each block
          bbMap[bbID] = new CSE_BBNode(bbID,B);
          bbID++;
				}
        //the start of every function is set to top
        Function::iterator first = F->begin();
        if(first != F->end()) {
          lookupBB(first->getName())->incoming->isTop = true;
        }
			}

      //setup the CSE_BBNodes with their outgoing edges now that they have ids
      for(map<int, CSE_BBNode*>::iterator it = bbMap.begin(); it != bbMap.end(); it++){
        it->second->setup();
      }
    }
    
    vector<CSE_Edge *> getIncomingEdges(CSE_BBNode* N){
      vector<CSE_Edge *> incoming_Edges;
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
      //1. create a vector of CSE_BBNodes
      vector<CSE_BBNode *> worklist;
      //2. enqueue each CSE_BBNode
      map<int, CSE_BBNode*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        worklist.push_back(it->second);
      }

      //print out starting results of top state for each block
      while(worklist.size() > 0) {
        //get any CSE_BBNode
        CSE_BBNode *currentCSE_Node = worklist.back();
        worklist.pop_back();
        
        //3. get incoming edges from predecessors
        vector<CSE_Edge *> incoming_Edges = getIncomingEdges(currentCSE_Node);
        //merge these incoming edges to form the node input
        CSE_Edge * in = optimizationLattice->merge(incoming_Edges);
        //set incoming edge to the new merged one
        free(currentCSE_Node->incoming);
        currentCSE_Node->incoming = in;
        CSE_Edge * inCopy = new CSE_Edge(in);
        
        //4. run flow function on CSE_BBNode
        map<int, CSE_Edge *> out = FF->m(inCopy,currentCSE_Node);
        
        //foreach output edge...
        map<int, CSE_Edge *>::iterator it;
        for(it = out.begin(); it != out.end(); it++) {
          int id = it->first;
          CSE_Edge * edgeOut = it->second;
          //5. check if the output edge has changed:
          if(optimizationLattice->comparator(edgeOut,currentCSE_Node->outgoing[id]) == false){
            free(currentCSE_Node->outgoing[id]);
            currentCSE_Node->outgoing[id] = edgeOut;
            CSE_BBNode * N = bbMap[id];
            vector<CSE_Edge *> succCSE_Edges = getIncomingEdges(N);
            CSE_Edge * result = optimizationLattice->merge(succCSE_Edges);
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
      errs() << "\n\n\nOutgoing edges:\n";
      //print out the relations for each block
      map<int, CSE_BBNode*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        CSE_BBNode * cBB = it->second;
        //print the BB id
        errs() << "-=BLOCK " << cBB->originalBB->getName() << " =-\n";
        //print output foreach successor
        map<int, CSE_Edge *>::iterator out;
        for(out = cBB->outgoing.begin(); out != cBB->outgoing.end(); out++) {
          errs() << "--Sucessor:  #" << out->first << "\n";
          CSE_Edge * cCSE_Edge = out->second;
          //print the input facts
          errs() << "\tCSE_Edge : { ";
          if(cCSE_Edge->isBottom == true){
            errs() << "Top }\n";
          } else {
            set<Instruction *>::iterator itr;
            for(itr = cCSE_Edge->aExpr.begin(); itr != cCSE_Edge->aExpr.end(); itr++) {
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
      map<int, CSE_BBNode*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        CSE_BBNode * cBB = it->second;
        CSE_Edge * in = cBB->incoming;
        //print the BB id
        errs() << "-=BLOCK " << cBB->originalBB->getName() << " =-\n";
        //print the input facts
        errs() << "CSE_Edge : { ";
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
