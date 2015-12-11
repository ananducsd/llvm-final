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

namespace metalattice {



/*
class EdgeFact {

	public:

	EdgeFact();
	EdgeFact(EdgeFact *p);
	bool isTop;
};

class Lattice {
	public:

	virtual bool comparator(EdgeFact * F1,EdgeFact * F2) = 0;
	virtual EdgeFact * merge(vector<EdgeFact *> edges) = 0;
};

template<typename E>
class FlowFunctions {
	public:

	virtual map<int, EdgeFact *> runFlowOnBlock(EdgeFact * in, BBNode<E> * N) = 0;
};

*/

// E is the concrete edge type

template<typename E>
class Node
{
  public:
  Instruction * I;
  // incoming edge
  E * e;

  Node(Instruction *pI){
    e = new E();
    I = pI;
  }
};

template<typename E>
class BBNode
{
  public:
  int bID;
  BasicBlock *originalBB;
  E *incoming;
  // List of successors for BB with a particular bbID
  map<int, E *> outgoing;
  vector<Node<E> *> nodes;

  BBNode(int ibID, BasicBlock * BB){
    bID = ibID;
    originalBB = BB;
    incoming = new E();
    //loop over each instruction in orginalBB and create a Node to point to them
    for (BasicBlock::iterator I = originalBB->begin(), Ie = originalBB->end(); I != Ie; ++I) {
      nodes.push_back(new Node<E>(I));
    }
  }
  //use the names of the successor BBs to add the outgoing edges
  void setup() {
    for (succ_iterator SI = succ_begin(originalBB), En = succ_end(originalBB); SI != En; ++SI) {
      BasicBlock *Succ = *SI;
      int id = atoi(Succ->getName().str().c_str());
      outgoing[id] = new E();
    }
  }
};



// E = concrete edge type, F = concrete flow function, L = concrete lattice
template<typename E, typename F, typename L>
class Worklist
{
  private:

    L* optimizationLattice;
    F* FF;

  public:
    map<int, BBNode<E> *> bbMap;
    //initialization: setup worklist components
    Worklist(){
      optimizationLattice = new L();
      FF = new F();
    }

    BBNode<E>* lookupBB(llvm::StringRef tempID){
      int id = atoi(tempID.str().c_str());
      return bbMap[id];
    }

    //connect references to BB and instructions
    //initialize all edges to bottom
    void init(llvm::Module &M){
      //iterate over all functions in module
      int bbID = 0;
	  for (Module::iterator Fb = M.begin(), Fe = M.end(); Fb != Fe; ++Fb){
		//iterate over all blocks in a function (that aren't the instrumented
        	for (Function::iterator B = Fb->begin(), Be = Fb->end(); B != Be; ++B) {
                //create a BBNode for each basic block
                B->setName(llvm::Twine(bbID));
                //create a BBNode for each block
                bbMap[bbID] = new BBNode<E>(bbID,B);
                bbID++;
			}
              //the start of every function is set to top
              Function::iterator first = Fb->begin();
              if(first != Fb->end()) {
                lookupBB(first->getName())->incoming->isTop = true;
            }
	}

      //setup the BBNodes with their outgoing edges now that they have ids
      for(typename map<int, BBNode<E> *>::iterator it = bbMap.begin(); it != bbMap.end(); it++){
        it->second->setup();
      }
    }

    vector<E *> getIncomingEs(BBNode<E>* N){
      vector<E *> incomingEs;
      int id = atoi(N->originalBB->getName().str().c_str());
      //3. get incoming edges from predecessors
      for (pred_iterator PI = pred_begin(N->originalBB), EI = pred_end(N->originalBB); PI != EI; ++PI) {
        //add to a vector of edges
        BasicBlock *Pred = *PI;
        incomingEs.push_back(lookupBB(Pred->getName())->outgoing[id]);
      }
      //if the block is the first in the function, then add that as well
      if(N->incoming->isTop == true){
        incomingEs.push_back(N->incoming);
      }
      return incomingEs;
    }

    //run does the actual worklist algorithm...
    void run(){
      //1. create a vector of BBNodes
      vector<BBNode<E> *> worklist;
      //2. enqueue each BBNode

      typename map<int, BBNode<E>*>::iterator it;
      for(it = bbMap.begin(); it != bbMap.end(); it++) {
        worklist.push_back(it->second);
      }

      while(worklist.size() > 0) {
        //get any BBNode
        BBNode<E> *currentNode = worklist.back();
        worklist.pop_back();

        //3. get incoming edges from predecessors
        vector<E *> incomingEs = getIncomingEs(currentNode);
        //merge these incoming edges to form the node input
        E * in = optimizationLattice->merge(incomingEs);
        //set incoming edge to the new merged one
        free(currentNode->incoming);
        currentNode->incoming = in;
        E * inCopy = new E(in);

        //4. run flow function on BBNode
        map<int, E *> out = FF->runFlowOnBlock(inCopy,currentNode);

        //foreach output edge...
        typename map<int, E *>::iterator it;
        for(it = out.begin(); it != out.end(); it++) {
          int id = it->first;
          E * edgeOut = it->second;
          //5. check if the output edge has changed:
          if(optimizationLattice->comparator(edgeOut,currentNode->outgoing[id]) == false){
            free(currentNode->outgoing[id]);
            currentNode->outgoing[id] = edgeOut;
            BBNode<E> * N = bbMap[id];
            vector<E *> succEs = getIncomingEs(N);
            E * result = optimizationLattice->merge(succEs);
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
