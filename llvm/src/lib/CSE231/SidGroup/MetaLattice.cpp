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

namespace metalattice{

class Edge {

	public: 

	Edge();
	Edge(BaseEdge *p);
	bool isTop;
};	

class Lattice {
	public:

	virtual bool comparator(Edge * F1,Edge * F2) = 0;
	virtual Edge * merge(vector<Edge *> edges) = 0; 
};

class FlowFunctions {
	public:

	virtual map<int, Edge *> m(Edge * in, BBNode * N) = 0;
};



class Node
{
  public:
  Instruction * I;
  // incoming edge
  Edge * e;
  
  Node(Instruction *pI){
    e = new Edge();
    I = pI;
  }
};

class BBNode
{
  public:
  int bID;
  BasicBlock *originalBB;
  Edge *incoming;
  map<int, Edge *> outgoing;
  vector<Node *> nodes;
  
  BBNode(int ibID, BasicBlock * BB){
    bID = ibID;
    originalBB = BB;
    incoming = new Edge();
    //loop over each instruction in orginalBB and create a Node to point to them
    for (BasicBlock::iterator I = originalBB->begin(), Ie = originalBB->end(); I != Ie; ++I) {
      nodes.push_back(new Node(I));
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


class Worklist
{
  private:
    
    Lattice* optimizationLattice;
    FlowFunctions* FF;
    
  public:
    map<int, BBNode*> bbMap;
    //initialization: setup worklist components
    Worklist(int type){
      optimizationLattice = new Lattice();
      FF = new FlowFunctions();
    }

    BBNode* lookupBB(llvm::StringRef tempID){
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
          //create a BBNode for each basic block
          B->setName(llvm::Twine(bbID));
          //create a BBNode for each block
          bbMap[bbID] = new BBNode(bbID,B);
          bbID++;
				}
        //the start of every function is set to top
        Function::iterator first = F->begin();
        if(first != F->end()) {
          lookupBB(first->getName())->incoming->isTop = true;
        }
	}

      //setup the BBNodes with their outgoing edges now that they have ids
      for(map<int, BBNode*>::iterator it = bbMap.begin(); it != bbMap.end(); it++){
        it->second->setup();
      }
    }
    
    vector<Edge *> getIncomingEdges(BBNode* N){
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
      //1. create a vector of BBNodes
      vector<BBNode *> worklist;
      //2. enqueue each BBNode
      map<int, BBNode*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        worklist.push_back(it->second);
      }

      while(worklist.size() > 0) {
        //get any BBNode
        BBNode *currentNode = worklist.back();
        worklist.pop_back();
        
        //3. get incoming edges from predecessors
        vector<Edge *> incomingEdges = getIncomingEdges(currentNode);
        //merge these incoming edges to form the node input
        Edge * in = optimizationLattice->merge(incomingEdges);
        //set incoming edge to the new merged one
        free(currentNode->incoming);
        currentNode->incoming = in;
        Edge * inCopy = new Edge(in);
        
        //4. run flow function on BBNode
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
            BBNode * N = bbMap[id];
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

    
};

}
