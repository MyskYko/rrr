#pragma once

#include <map>
#include <iterator>
#include <random>
#include <numeric>

#include "rrrParameter.h"
#include "rrrUtils.h"

namespace rrr {

  // it is assumed that removing redundancy never degrades the cost in this optimizer
  template <typename Ntk, typename Ana>
  class Optimizer2 {
  private:
    // parameters
    int nVerbose;
    int nSortTypeOriginal;
    int nSortType;
    int nFlow;
    int nDistance;
    bool fCompatible;
    bool fGreedy;
    seconds nTimeout; // assigned upon Run
    std::function<void(std::string)> PrintLine;

    // data
    Ana ana;

    // marks
    unsigned iTrav;
    std::vector<unsigned> vTrav;

    // print
    template<typename... Args>
    void Print(int nVerboseLevel, Args... args);

    // topology
    unsigned StartTraversal(int nNodes, int n = 1);

  public:
    // constructors
    Optimizer2(Parameter const *pPar);
    void SetPrintLine(std::function<void(std::string)> const &PrintLine_);

    // run
    void Prepare(Ntk *pNtk);
    std::vector<std::pair<Ntk *, std::vector<Action>>> Run(std::pair<Ntk *, std::vector<Action>> const &input, bool fAdd);
  };


  /* {{{ Print */

  template <typename Ntk, typename Ana>
  template <typename... Args>
  inline void Optimizer2<Ntk, Ana>::Print(int nVerboseLevel, Args... args) {
    if(nVerbose > nVerboseLevel) {
      std::stringstream ss;
      for(int i = 0; i < nVerboseLevel; i++) {
        ss << "\t";
      }
      PrintNext(ss, args...);
      PrintLine(ss.str());
    }
  }
  
  /* }}} */

  /* {{{ Topology */
  
  template <typename Ntk, typename Ana>
  inline unsigned Optimizer2<Ntk, Ana>::StartTraversal(int nNodes, int n) {
    do {
      for(int i = 0; i < n; i++) {
        iTrav++;
        if(iTrav == 0) {
          vTrav.clear();
          break;
        }
      }
    } while(iTrav == 0);
    vTrav.resize(nNodes);
    return iTrav - n + 1;
  }

  /* }}} */
  
  /* {{{ Constructors */
  
  template <typename Ntk, typename Ana>
  Optimizer2<Ntk, Ana>::Optimizer2(Parameter const *pPar) :
    nVerbose(pPar->nOptimizerVerbose),
    nSortTypeOriginal(pPar->nSortType),
    nSortType(pPar->nSortType),
    nFlow(pPar->nOptimizerFlow),
    nDistance(pPar->nDistance),
    fCompatible(pPar->fUseBddCspf),
    fGreedy(pPar->fGreedy),
    ana(pPar),
    iTrav(0) {
  }
  
  template <typename Ntk, typename Ana>
  void Optimizer2<Ntk, Ana>::SetPrintLine(std::function<void(std::string)> const &PrintLine_) {
    PrintLine = PrintLine_;
  }
  
  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Ana>
  void Optimizer2<Ntk, Ana>::Prepare(Ntk *pNtk) {
    bool fReuse = true;
    ana.AssignNetwork(pNtk, fReuse);
    std::vector<int> vInts = pNtk->GetInts();
    for(std::vector<int>::const_reverse_iterator it = vInts.rbegin(); it != vInts.rend();) {
      if(!pNtk->IsInt(*it)) {
        it++;
        continue;
      }
      if(pNtk->GetNumFanouts(*it) == 0) {
        pNtk->RemoveUnused(*it);
        it++;
        continue;
      }
      bool fReduced = false;
      for(int idx = 0; idx < pNtk->GetNumFanins(*it); idx++) {
        if(ana.CheckRedundancy(*it, idx)) {
          pNtk->RemoveFanin(*it, idx);
          fReduced = true;
          idx--;
        }
      }
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
      }
      if(fReduced) {
        it = vInts.rbegin();
      } else {
        it++;
      }
    }
    pNtk->TrivialCollapse();
  }

  template <typename Ntk, typename Ana>
  std::vector<std::pair<Ntk *, std::vector<Action>>> Optimizer2<Ntk, Ana>::Run(std::pair<Ntk *, std::vector<Action>> const &input, bool fAdd) {
    Ntk *pNtk = input.first;
    bool fReuse = true;
    ana.AssignNetwork(pNtk, fReuse);
    std::vector<std::pair<Ntk *, std::vector<Action>>> vResults;
    if(fAdd) {
      std::vector<int> vCands = pNtk->GetPisInts();
      pNtk->ForEachInt([&](int id) {
        StartTraversal(pNtk->GetNumNodes());
        pNtk->ForEachTfo(id, false, [&](int fo) {
          vTrav[fo] = iTrav;
        });
        vTrav[id] = iTrav;
        pNtk->ForEachFanin(id, [&](int fi) {
          vTrav[fi] = iTrav;
        });
        for(int fi: vCands) {
          if(vTrav[fi] == iTrav) {
            continue;
          }
          if(ana.CheckFeasibility(id, fi, false)) {
            Ntk *pNew = new Ntk(*pNtk);
            std::vector<Action> vActions = input.second;
            int index = pNew->AddCallback([&](Action const &action) {
              vActions.push_back(action);
            });
            pNew->AddFanin(id, fi, false);
            vResults.emplace_back(pNew, vActions);
            pNew->DeleteCallback(index);
            //std::cout << "adding " << id << " " << fi << std::endl;
          } else if(pNtk->UseComplementedEdges() && ana.CheckFeasibility(id, fi, true)) {
            Ntk *pNew = new Ntk(*pNtk);
            std::vector<Action> vActions = input.second;
            int index = pNew->AddCallback([&](Action const &action) {
              vActions.push_back(action);
            });
            pNew->AddFanin(id, fi, true);
            vResults.emplace_back(pNew, vActions);
            pNew->DeleteCallback(index);
            //std::cout << "adding " << id << " !" << fi << std::endl;
          }
        }
      });
    } else {
      assert(!input.second.empty());
      int added_id = -1;
      int added_fi = -1;
      if(input.second.back().type == ADD_FANIN) {
        added_id = input.second.back().id;
        added_fi = input.second.back().fi;
      }
      //std::cout << "blocking " << added_id << " " << added_fi << std::endl;
      pNtk->ForEachInt([&](int id) {
        assert(pNtk->GetNumFanouts(id) > 0);
        for(int idx = 0; idx < pNtk->GetNumFanins(id); idx++) {
          // skip fanins that were just added
          if(id == added_id && pNtk->GetFanin(id, idx) == added_fi) {
            continue;
          }
          if(ana.CheckRedundancy(id, idx)) {
            Ntk *pNew = new Ntk(*pNtk);
            std::vector<Action> vActions = input.second;
            int index = pNew->AddCallback([&](Action const &action) {
              vActions.push_back(action);
            });
            //std::cout << "deleting " << id << " " << pNtk->GetFanin(id, idx) << std::endl;
            pNew->RemoveFanin(id, idx);
            if(pNew->GetNumFanins(id) <= 1) {
              pNew->Propagate(id);
            }
            pNew->Sweep(false);
            pNew->TrivialCollapse();
            vResults.emplace_back(pNew, vActions);
            pNew->DeleteCallback(index);
          }
        }
      });
    }
    return vResults;
  }
  
  /* }}} */
  
}
