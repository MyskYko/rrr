#pragma once

#include <map>
#include <iterator>
#include <random>
#include <numeric>

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"

namespace rrr {

  // it is assumed that removing redundancy never degrades the cost in this optimizer
  template <typename Ntk, typename Ana>
  class UrOptimizer {
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
    Ntk *pNtk;
    Ana ana;
    int slot;
    bool fAdd;
    int nChoices;
    int nFaninChoices;
    std::vector<Action> vFaninActions;
    int nCurrentFanin;

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
    UrOptimizer(Parameter const *pPar);
    void AssignNetwork(Ntk *pNtk_, bool fAdd_);
    void SetPrintLine(std::function<void(std::string)> const &PrintLine_);

    // run
    bool IsRedundant(Ntk *pNtk_);
    std::vector<Action> Run();
  };

  /* {{{ Print */

  template <typename Ntk, typename Ana>
  template <typename... Args>
  inline void UrOptimizer<Ntk, Ana>::Print(int nVerboseLevel, Args... args) {
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
  inline unsigned UrOptimizer<Ntk, Ana>::StartTraversal(int nNodes, int n) {
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
  UrOptimizer<Ntk, Ana>::UrOptimizer(Parameter const *pPar) :
    nVerbose(pPar->nOptimizerVerbose),
    nSortTypeOriginal(pPar->nSortType),
    nSortType(pPar->nSortType),
    nFlow(pPar->nOptimizerFlow),
    nDistance(pPar->nDistance),
    fCompatible(pPar->fUseBddCspf),
    fGreedy(pPar->fGreedy),
    ana(pPar),
    slot(-1),
    nCurrentFanin(-1),
    iTrav(0) {
  }
  
  template <typename Ntk, typename Ana>
  void UrOptimizer<Ntk, Ana>::AssignNetwork(Ntk *pNtk_, bool fAdd_) {
    assert(slot == -1);
    nCurrentFanin = -1;
    pNtk = pNtk_;
    fAdd = fAdd_;
    ana.AssignNetwork(pNtk, true /* fReuse */);
    slot = pNtk->Save();
    assert(slot == 0); // do not allow external saves
    nChoices = 0;
    nFaninChoices = 0;
    assert(vFaninActions.empty());
  }
  
  template <typename Ntk, typename Ana>
  void UrOptimizer<Ntk, Ana>::SetPrintLine(std::function<void(std::string)> const &PrintLine_) {
    PrintLine = PrintLine_;
  }

  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Ana>
  bool UrOptimizer<Ntk, Ana>::IsRedundant(Ntk *pNtk_) {
    assert(slot == -1);
    ana.AssignNetwork(pNtk_, true /* fReuse */);
    bool fRedundant = false;
    pNtk_->ForEachIntStop([&](int id) {
      assert(pNtk_->GetNumFanouts(id) > 0);
      for(int idx = 0; idx < pNtk_->GetNumFanins(id); idx++) {
        if(ana.CheckRedundancy(id, idx)) {
          fRedundant = true;
          return true;
        }
      }
      return false;
    });
    return fRedundant;
  }

  template <typename Ntk, typename Ana>
  std::vector<Action> UrOptimizer<Ntk, Ana>::Run() {
    if(fAdd) {
      Print(0, "loading", slot, ":", "fan", nFaninChoices, ",", "tar", nChoices);
    } else {
      Print(0, "loading", slot, ":", "red", nChoices);
    }
    pNtk->Load(slot);
    if(!fAdd) {
      Print(1, "reduction", ":", "red", "=", nChoices);
      // perform reduction
      std::vector<Action> vActions;
      int counter = 0;
      pNtk->ForEachIntStop([&](int id) {
        assert(pNtk->GetNumFanouts(id) > 0);
        for(int idx = 0; idx < pNtk->GetNumFanins(id); idx++) {
          // TODO: skip fanins that were just added? we may get that info from actions or let the add routine return something
          counter++;
          if(counter > nChoices) {
            if(ana.CheckRedundancy(id, idx)) {
              Print(0, "deleting", "node", id, ",", "fanin", pNtk->GetFanin(id, idx));
              nChoices = counter;
              int index = pNtk->AddCallback([&](Action const &action) {
                vActions.push_back(action);
              });
              pNtk->RemoveFanin(id, idx);
              if(pNtk->GetNumFanins(id) <= 1) {
                pNtk->Propagate(id);
              }
              pNtk->Sweep();
              while(pNtk->TrivialCollapse()) {
                pNtk->Sweep(true);
              }
              pNtk->DeleteCallback(index);
              return true;
            }
          }
        }
        return false;
      });
      if(vActions.empty()) {
        pNtk->PopBack();
        slot--;
      }
      return vActions;
    }
    // add
    while(true) {
      if(nCurrentFanin != -1) {
        bool fAdded = false;
        std::vector<Action> vActions = vFaninActions;
        int counter = 0;
        StartTraversal(pNtk->GetNumNodes());
        pNtk->ForEachTfi(nCurrentFanin, false, [&](int fi) {
          vTrav[fi] = iTrav;
        });
        vTrav[nCurrentFanin] = iTrav;
        pNtk->ForEachFanout(nCurrentFanin, false, [&](int fo) {
          vTrav[fo] = iTrav;
        });
        pNtk->ForEachIntStop([&](int id) {
          if(vTrav[id] == iTrav) {
            return false;
          }
          bool fAdding = false;
          bool c = false;
          counter++;
          if(counter > nChoices) {
            nChoices = counter;
            if(ana.CheckFeasibility(id, nCurrentFanin, false)) {
              fAdding = true;
              c = false;
            }
          }
          if(!fAdding && pNtk->UseComplementedEdges()) {
            counter++;
            if(counter > nChoices) {
              nChoices = counter;
              if(ana.CheckFeasibility(id, nCurrentFanin, true)) {
                fAdding = true;
                c = true;
              }
            }
          }
          if(fAdding) {
            Print(0, "adding", "node", id, ",", "fanin", (c? "!": ""), nCurrentFanin);
            int index = pNtk->AddCallback([&](Action const &action) {
              vActions.push_back(action);
            });
            pNtk->AddFanin(id, nCurrentFanin, c);
            pNtk->DeleteCallback(index);
            fAdded = true;
            return true;
          }
          return false;
        });
        if(fAdded) {
          return vActions;
        }
        nCurrentFanin = -1;
        nChoices = 0;
        vFaninActions.clear();
        pNtk->PopBack();
        slot--;
        Print(0, "loading", slot, ":", "fan", nFaninChoices, ",", "tar", nChoices);
        pNtk->Load(slot);
      } else {
        bool fFound = false;
        int counter = 0;
        pNtk->ForEachPiIntStop([&](int id) {
          if(pNtk->GetNumFanins(id) > 2) {
            int nFanins = pNtk->GetNumFanins(id);
            assert(nFanins < 31);
            int nDecChoices = 1 << nFanins;
            nDecChoices -= nFanins + 1;
            if(counter + nDecChoices <= nFaninChoices) {
              counter += nDecChoices;
              return false;
            }
            for(unsigned i = 0; i < (1u << nFanins); i++) {
              if((i & (i - 1)) == 0) { // nSelected < 2
                continue;
              }
              counter++;
              if(counter > nFaninChoices) {
                nFaninChoices = counter;
                int fi = id;
                if(i + 1u < (1u << nFanins)) { // nSelected < nFanins
                  int nSelected = __builtin_popcount(i);
                  std::vector<int> vIndices;
                  vIndices.reserve(nFanins);
                  for(int j = 0; j < nFanins; j++) {
                    if(((i >> (nFanins - j - 1)) & 1) == 0) {
                      vIndices.push_back(j);
                    }
                  }
                  assert(int_size(vIndices) == nFanins - nSelected);
                  for(int j = 0; j < nFanins; j++) {
                    if((i >> (nFanins - j - 1)) & 1) {
                      vIndices.push_back(j);
                    }
                  }
                  assert(int_size(vIndices) == nFanins);
                  int index = pNtk->AddCallback([&](Action const &action) {
                    vFaninActions.push_back(action);
                  });
                  pNtk->SortFanins(id, vIndices);
                  fi = pNtk->TrivialDecompose(id, nSelected);
                  pNtk->DeleteCallback(index);
                  Print(0, "decomposing", "node", id, ",", "indices", vIndices, ",", "fanin", fi);
                }
                nCurrentFanin = fi;
                Print(0, "using", "fanin", nCurrentFanin);
                slot = pNtk->Save();
                Print(0, "saving", slot);
                fFound = true;
                return true;
              }
            }
          } else {
            counter++;
            if(counter > nFaninChoices) {
              nFaninChoices = counter;
              nCurrentFanin = id;
              Print(0, "using", "fanin", nCurrentFanin);
              slot = pNtk->Save();
              Print(0, "saving", slot);
              fFound = true;
              return true;
            }
          }
          return false;
        });
        if(!fFound) {
          pNtk->PopBack();
          slot--;
          return vFaninActions;
        }
      }
    }
  }
  
  /* }}} */
  
}
