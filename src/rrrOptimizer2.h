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
    Ntk *pNtk;
    bool fAdd;
    Ana ana;
    int slot;
    int nAddChoice;
    std::vector<int> vRedChoices;
    std::vector<Action> vActions;
    std::vector<std::vector<Action>> vvActions;
    int nAddedId;
    int nAddedFi;
    std::vector<int> vCands;

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
    void AssignNetwork(Ntk *pNtk_, bool fAdd_);
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
    iTrav(0) {
  }
  
  template <typename Ntk, typename Ana>
  void Optimizer2<Ntk, Ana>::AssignNetwork(Ntk *pNtk_, bool fAdd_) {
    assert(slot == -1);
    pNtk = pNtk_;
    fAdd = fAdd_;
    ana.AssignNetwork(pNtk, true /* fReuse */);
    slot = pNtk->Save();
    assert(slot == 0); // do not allow external saves
    nAddChoice = 0;
    assert(vRedChoices.empty());
    if(!fAdd) {
      vRedChoices.push_back(0);
    }
    vActions.clear();
    assert(vvActions.empty());
    vvActions.push_back(vActions);
    nAddedId = -1;
    nAddedFi = -1;
    vCands = pNtk->GetPisInts();
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
    //std::cout << "loading " << slot << std::endl;
    pNtk->Load(slot);
    vActions = vvActions.back();
    while(true) {
      if(!vRedChoices.empty()) {
        //std::cout << "reduction: " << "slot = " << slot << ", vRedChoices = " << vRedChoices << ", |vActions| = " << vActions.size() << std::endl;
        // perform reduction
        bool fReduced = false;
        int counter = 0;
        pNtk->ForEachIntStop([&](int id) {
          assert(pNtk->GetNumFanouts(id) > 0);
          for(int idx = 0; idx < pNtk->GetNumFanins(id); idx++) {
            // skip fanins that were just added
            if(vRedChoices.size() == 1 && id == nAddedId && pNtk->GetFanin(id, idx) == nAddedFi) {
              continue;
            }
            counter++;
            if(counter > vRedChoices.back()) {
              if(ana.CheckRedundancy(id, idx)) {
                //std::cout << "deleting " << id << " " << pNtk->GetFanin(id, idx) << std::endl;
                vRedChoices.back() = counter;
                int index = pNtk->AddCallback([&](Action const &action) {
                  vActions.push_back(action);
                });
                pNtk->RemoveFanin(id, idx);
                if(pNtk->GetNumFanins(id) <= 1) {
                  pNtk->Propagate(id);
                }
                pNtk->Sweep(false);
                pNtk->TrivialCollapse();
                pNtk->DeleteCallback(index);
                slot = pNtk->Save();
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
            break;
          }
          //std::cout << "loading " << slot << std::endl;
          pNtk->Load(slot);
          vActions = vvActions.back();
        }
      } else {
        assert(fAdd);
        bool fAdded = false;
        int counter = 0;
        pNtk->ForEachIntStop([&](int id) {
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
            bool fAdding = false;
            bool c = false;
            counter++;
            if(counter > nAddChoice) {
              if(ana.CheckFeasibility(id, fi, false)) {
                //std::cout << "feasible " << id << " " << fi << std::endl;
                fAdding = true;
                c = false;
              }
            }
            if(!fAdding && pNtk->UseComplementedEdges()) {
              counter++;
              if(counter > nAddChoice) {
                if(ana.CheckFeasibility(id, fi, true)) {
                  //std::cout << "feasible " << id << " !" << fi << std::endl;
                  fAdding = true;
                  c = true;
                }
              }
            }
            if(fAdding) {
              //std::cout << "adding " << id << " " << (c? "!": "") << fi << std::endl;
              nAddChoice = counter;
              int index = pNtk->AddCallback([&](Action const &action) {
                vActions.push_back(action);
              });
              pNtk->AddFanin(id, fi, c);
              pNtk->DeleteCallback(index);
              nAddedId = id;
              nAddedFi = fi;
              slot = pNtk->Save();
              vvActions.push_back(vActions);
              vRedChoices.push_back(0);
              fAdded = true;
              return true;
            }
          }
          return false;
        });
        if(!fAdded) {
          vvActions.pop_back();
          pNtk->PopBack();
          slot--;
          break;
        }
      }
    }
    return vActions;
  }
  
  /* }}} */
  
}
