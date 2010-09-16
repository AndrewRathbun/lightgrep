#pragma once

#include "utility.h"
#include "instructions.h"

#include <vector>
#include <iostream>
#include <cstring>
#include <exception>
#include <algorithm>

#include <boost/graph/breadth_first_search.hpp>

static const uint32 UNALLOCATED = 0xffffffff;

struct StateLayoutInfo {
  uint32 Start,
         NumEval,
         NumOther,
         CheckIndex;
  bool   HasChecks;

  StateLayoutInfo(): Start(UNALLOCATED), NumEval(UNALLOCATED), NumOther(UNALLOCATED), CheckIndex(UNALLOCATED), HasChecks(false) {}
  StateLayoutInfo(uint32 s, uint32 e, uint32 o): Start(s), NumEval(e), NumOther(o), CheckIndex(UNALLOCATED), HasChecks(false) {}

  uint32 numTotal() const { return NumEval + NumOther; }
};

struct CodeGenHelper {
  CodeGenHelper(uint32 numStates): DiscoverRanks(numStates, UNALLOCATED), Snippets(numStates), Guard(0), NumDiscovered(0), NumChecked(0) {}

  void discover(DynamicFSM::vertex_descriptor v, const DynamicFSM& graph) {
    DiscoverRanks[v] = NumDiscovered++;
    if (boost::in_degree(v, graph) > 1) {
      Snippets[v].CheckIndex = ++NumChecked;
      InEdgeRange in(boost::in_edges(v, graph));
      for (InEdgeIt e(in.first); e != in.second; ++e) {
        Snippets[boost::source(*e, graph)].HasChecks = true;
      }
    }
  }

  void addSnippet(uint32 state, uint32 numEval, uint32 numOther) {
    StateLayoutInfo& info(Snippets[state]);
    info.Start = Guard;
    info.NumEval = numEval;
    info.NumOther = numOther;
    Guard += info.numTotal();
  }

  std::vector< uint32 > DiscoverRanks;
  std::vector< StateLayoutInfo > Snippets;
  uint32 Guard,
         NumDiscovered,
         NumChecked;
};

typedef std::vector< std::vector< DynamicFSM::vertex_descriptor > > TransitionTbl;

class CodeGenVisitor: public boost::default_bfs_visitor {
public:
  CodeGenVisitor(boost::shared_ptr<CodeGenHelper> helper): Helper(helper) {}

  void discover_vertex(DynamicFSM::vertex_descriptor v, const DynamicFSM& graph) {
    Helper->discover(v, graph);
  }

  static bool shouldBeJumpTable(DynamicFSM::vertex_descriptor v, const DynamicFSM& graph, uint32 outDegree, uint32& totalSize) {
    if (outDegree > 3 && (v == 0 || graph[v]->Label == UNALLOCATED)) {
      TransitionTbl tbl(pivotStates(v, graph));
      if (maxOutbound(tbl) < outDegree) {
        uint32 sizeIndirectTables = 0,
                num;
        for (uint32 i = 0; i < 256; ++i) {
          num = tbl[i].size();
          if (num > 1) {
            sizeIndirectTables += num;
          }
        }
        totalSize = 257 + sizeIndirectTables;
        return true;
      }
    }
    totalSize = 0;
    return false;
  }

  void finish_vertex(DynamicFSM::vertex_descriptor v, const DynamicFSM& graph) {
    // std::cerr << "on state " << v << " with discover rank " << Helper->DiscoverRanks[v] << std::endl;
    uint32 labels = 0,
           eval   = (v == 0 ? 0: graph[v]->numInstructions()),
           outDegree = out_degree(v, graph),
           totalSize;
    if (shouldBeJumpTable(v, graph, outDegree, totalSize)) {
      Helper->addSnippet(v, eval, totalSize);
      return;
    }
/*    if ((0 == v && outDegree > 3) || (outDegree > 3 && graph[v]->Label == UNALLOCATED && !Helper->Snippets[v].HasChecks && maxOutbound(pivotStates(v, graph)) < outDegree)) {
      std::vector< std::vector< DynamicFSM::vertex_descriptor > > tbl(pivotStates(v, graph));
      uint32 sizeIndirectTables = 0;
      for (uint32 i = 0; i < 256; ++i) {
        uint32 num = tbl[i].size();
        if (num > 1) {
          sizeIndirectTables += num;
        }
      }
      Helper->addSnippet(v, eval, 257 + sizeIndirectTables);
      return;
    }*/
    if (graph[v] && graph[v]->Label < 0xffffffff) {
      ++labels;
    }
    uint32 outOps = 0;
    OutEdgeRange outRange(out_edges(v, graph));
    if (outRange.first == outRange.second) {
      // std::cerr << "no out edges, so a halt" << std::endl;
      outOps = 1; // HALT instruction
    }
    else {
      for (OutEdgeIt curOut(outRange.first); curOut != outRange.second; ++curOut) {
        // if a target state immediately follows the current state, then we don't need an instruction for it
        if (Helper->DiscoverRanks[v] + 1 != Helper->DiscoverRanks[target(*curOut, graph)]) {
          ++outOps;
        }
        if (boost::in_degree(target(*curOut, graph), graph) > 1) {
          ++outOps;
        }
      }
    }
    // std::cerr << "outOps = " << outOps << "; labels = " << labels << "; match = " << isMatch << std::endl;
    Helper->addSnippet(v, eval, outOps + labels);
    // std::cerr << "state " << v << " has snippet " << "(" << Helper->Snippets[v].first << ", " << Helper->Snippets[v].second << ")" << std::endl;
  }

private:
  boost::shared_ptr<CodeGenHelper> Helper;
};

template<class VisitorT>
void specialVisit(const DynamicFSM& graph, DynamicFSM::vertex_descriptor startVertex, VisitorT& vis) {
  std::deque< DynamicFSM::vertex_descriptor > statesToVisit;
  std::vector< DynamicFSM::vertex_descriptor > inOrder;
  std::vector< bool > discovered(boost::num_vertices(graph), false);

  discovered[startVertex].flip();
  // vis.discover_vertex(startVertex, graph);
  statesToVisit.push_back(startVertex);
  while (!statesToVisit.empty()) {
    DynamicFSM::vertex_descriptor v = statesToVisit.front();
    statesToVisit.pop_front();

    vis.discover_vertex(v, graph);
    inOrder.push_back(v);

    uint32 numOut = boost::out_degree(v, graph);
    OutEdgeRange outRange = boost::out_edges(v, graph);
    // InEdgeRange  inRange  = boost::in_edges(v, graph);
    bool front = numOut < 2;// && boost::in_degree(v, graph) == 1 && boost::out_degree(boost::source(*inRange.first, graph), graph) == 1;
    for (OutEdgeIt curOut(outRange.first); curOut != outRange.second; ++curOut) {
      DynamicFSM::vertex_descriptor t = boost::target(*curOut, graph);
      if (!discovered[t]) {
        discovered[t].flip();
        // vis.discover_vertex(t, graph);
        if (front) {
          statesToVisit.push_front(t);
        }
        else {
          statesToVisit.push_back(t);
        }
      }
    }
    // vis.finish_vertex(v, graph);
  }
  for (uint32 i = 0; i < inOrder.size(); ++i) {
    vis.finish_vertex(inOrder[i], graph);
  }
}
