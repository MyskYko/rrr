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
    Ntk *pNtk;
    int nAdd;
    Ana ana;
    int slot;
    int nAddChoice;
    std::vector<int> vFaninChoices;
    std::vector<int> vTargetChoices;
    std::vector<int> vRedChoices;
    std::vector<Action> vActions;
    std::vector<std::vector<Action>> vvActions;
    std::map<int, std::set<int>> mapNewFanins;
    std::vector<int> vAddedIds;
    std::vector<int> vAddedFis;
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
    Optimizer2(Parameter const *pPar);
    void AssignNetwork(Ntk *pNtk_, int nAdd_);
    void SetPrintLine(std::function<void(std::string)> const &PrintLine_);

    // run
    bool IsRedundant(Ntk *pNtk_);
    std::vector<Action> Run();
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
    slot(-1),
    nCurrentFanin(-1),
    iTrav(0) {
  }
  
  template <typename Ntk, typename Ana>
  void Optimizer2<Ntk, Ana>::AssignNetwork(Ntk *pNtk_, int nAdd_) {
    assert(slot == -1);
    nCurrentFanin = -1;
    pNtk = pNtk_;
    nAdd = nAdd_;
    ana.AssignNetwork(pNtk, true /* fReuse */);
    slot = pNtk->Save();
    assert(slot == 0); // do not allow external saves
    nAddChoice = 0;
    assert(vFaninChoices.empty());
    if(nAdd) {
      vFaninChoices.push_back(0);
    }
    assert(vTargetChoices.empty());
    assert(vRedChoices.empty());
    if(!nAdd) {
      vRedChoices.push_back(0);
    }
    vActions.clear();
    assert(vvActions.empty());
    vvActions.push_back(vActions);
    assert(mapNewFanins.empty());
  }
  
  template <typename Ntk, typename Ana>
  void Optimizer2<Ntk, Ana>::SetPrintLine(std::function<void(std::string)> const &PrintLine_) {
    PrintLine = PrintLine_;
  }

  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Ana>
  bool Optimizer2<Ntk, Ana>::IsRedundant(Ntk *pNtk_) {
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
  std::vector<Action> Optimizer2<Ntk, Ana>::Run() {
    Print(0, "loading", slot, ":", "fan", vFaninChoices, ",", "tar", vTargetChoices, "," "red", vRedChoices);
    pNtk->Load(slot);
    vActions = vvActions.back();
    // TODO: can we somehow skip combinations we already tried? (rm a -> rm b) and (rm b -> rm a) are the same
    while(true) {
      if(!vRedChoices.empty()) {
        Print(1, "reduction", ":", "slot", "=", slot, ",", "vRedChoices", "=", vRedChoices, ",", "|Actions|", "=", vActions.size());
        // perform reduction
        bool fReduced = false;
        int counter = 0;
        pNtk->ForEachIntStop([&](int id) {
          assert(pNtk->GetNumFanouts(id) > 0);
          for(int idx = 0; idx < pNtk->GetNumFanins(id); idx++) {
            // skip fanins that were just added
            if(vRedChoices.size() == 1) {
              // TODO: we shouldn't limit to size() == 1 when multiple edges have been added
              bool fSkip = false;
              for(int i = 0; i < int_size(vAddedIds); i++) {
                if(vAddedIds[i] == id && vAddedFis[i] == pNtk->GetFanin(id, idx)) {
                  fSkip = true;
                  break;
                }
              }
              if(fSkip) {
                continue;
              }
            }
            counter++;
            if(counter > vRedChoices.back()) {
              if(ana.CheckRedundancy(id, idx)) {
                Print(0, "deleting", "node", id, ",", "fanin", pNtk->GetFanin(id, idx));
                vRedChoices.back() = counter;
                int index = pNtk->AddCallback([&](Action const &action) {
                  vActions.push_back(action);
                });
                pNtk->RemoveFanin(id, idx);
                if(pNtk->GetNumFanins(id) <= 1) {
                  pNtk->Propagate(id);
                }
                pNtk->Sweep(false);
                //pNtk->TrivialCollapse();
                pNtk->DeleteCallback(index);
                slot = pNtk->Save();
                Print(0, "saving", slot);
                vvActions.push_back(vActions);
                vRedChoices.push_back(0);
                fReduced = true;
                return true;
              }
            }
          }
          return false;
        });
        if(!fReduced) {
          int nRedChoice = vRedChoices.back();
          vRedChoices.pop_back();
          vvActions.pop_back();
          pNtk->PopBack();
          slot--;
          assert(slot >= 0 || vRedChoices.empty());
          assert(slot >= 0 || nRedChoice); // a given redundant circuit must not be irredundant
          if(slot < 0 || (!nRedChoice && !vRedChoices.empty())) {
            // if slot < 0, we have searched all possibilities starting from a given redundant circuit
            // if slot >= 0 and !nRedChoice and !vRedChoices.empty(), nothing was redundant i.e. the circuit is irredundant
            // if slot >= 0 and !nRedChoice but vRedChoices.empty(), nothing was redundant except the added one, so we
            // continue to the next addition by loading the previous one
            int index = pNtk->AddCallback([&](Action const &action) {
              vActions.push_back(action);
            });
            pNtk->TrivialCollapse();
            pNtk->DeleteCallback(index);
            break;
          }
          Print(0, "loading", slot, ":", "fan", vFaninChoices, ",", "tar", vTargetChoices, "," "red", vRedChoices);
          pNtk->Load(slot);
          vActions = vvActions.back();
          if(vRedChoices.empty()) {
            vAddedIds.pop_back();
            vAddedFis.pop_back();
          }
        }
      } else {
        assert(nAdd);
        if(vTargetChoices.size() == vFaninChoices.size()) {
          bool fAdded = false;
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
            if(counter > vTargetChoices.back()) {
              vTargetChoices.back() = counter;
              if(ana.CheckFeasibility(id, nCurrentFanin, false)) {
                fAdding = true;
                c = false;
              }
            }
            if(!fAdding && pNtk->UseComplementedEdges()) {
              counter++;
              if(counter > vTargetChoices.back()) {
                vTargetChoices.back() = counter;
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
              vAddedIds.push_back(id);
              vAddedFis.push_back(nCurrentFanin);
              slot = pNtk->Save();
              Print(0, "saving", slot);
              vvActions.push_back(vActions);
              if(int_size(vTargetChoices) == nAdd) {
                vRedChoices.push_back(0);
              } else {
                vFaninChoices.push_back(0);
              }
              fAdded = true;
              return true;
            }
            return false;
          });
          if(!fAdded) {
            vTargetChoices.pop_back();
            vvActions.pop_back();
            pNtk->PopBack();
            slot--;
            Print(0, "loading", slot, ":", "fan", vFaninChoices, ",", "tar", vTargetChoices, "," "red", vRedChoices);
            pNtk->Load(slot);
            vActions = vvActions.back();
          }
        } else {
          bool fFound = false;
          int counter = 0;
          pNtk->ForEachPiIntStop([&](int id) {
            if(pNtk->GetNumFanins(id) > 2) {
              int nFanins = pNtk->GetNumFanins(id);
              assert(nFanins < 31);
              int nChoices = 1 << nFanins;
              nChoices -= nFanins + 1;
              if(counter + nChoices <= vFaninChoices.back()) {
                counter += nChoices;
                return false;
              }
              for(unsigned i = 0; i < (1u << nFanins); i++) {
                int nSelected = __builtin_popcount(i);
                if(nSelected < 2) {
                  continue;
                }
                counter++;
                if(counter > vFaninChoices.back()) {
                  vFaninChoices.back() = counter;
                  int fi = id;
                  if(i + 1u < 1u << nFanins) {
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
                      vActions.push_back(action);
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
                  vvActions.push_back(vActions);
                  vTargetChoices.push_back(0);
                  fFound = true;
                  return true;
                }
              }
            } else {
              counter++;
              if(counter > vFaninChoices.back()) {
                vFaninChoices.back() = counter;
                nCurrentFanin = id;
                Print(0, "using", "fanin", nCurrentFanin);
                slot = pNtk->Save();
                Print(0, "saving", slot);
                vvActions.push_back(vActions);
                vTargetChoices.push_back(0);
                fFound = true;
                return true;
              }
            }
            return false;
          });
          if(!fFound) {
            vFaninChoices.pop_back();
            vvActions.pop_back();
            pNtk->PopBack();
            slot--;
            if(slot < 0) {
              break;
            }
            Print(0, "loading", slot, ":", "fan", vFaninChoices, ",", "tar", vTargetChoices, "," "red", vRedChoices);
            pNtk->Load(slot);
            vActions = vvActions.back();
            vAddedIds.pop_back();
            vAddedFis.pop_back();
          }
        }
      }
    }
    return vActions;
  }
  
  /* }}} */
  
}
