#pragma once

#include <vector>
#include <map>
#include <iterator>
#include <random>
#include <numeric>

#include "rrrTypes.h"
#include "rrrParameter.h"

namespace rrr {

  template <typename Ntk, typename Ana>
  class Optimizer {
  private:
    // aliases
    using itr = std::vector<int>::iterator;
    using citr = std::vector<int>::const_iterator;
    using ritr = std::vector<int>::reverse_iterator;
    using critr = std::vector<int>::const_reverse_iterator;

    // pointer to network
    Ntk *pNtk;
    
    // parameters
    int nVerbose;
    std::function<double(Ntk *)> CostFunction;
    seconds nTimeout; // assigned upon Run

    // data
    Ana *pAna;
    std::mt19937 rng;
    std::vector<int> vTmp;
    std::map<int, std::set<int>> mapNewFanins;
    time_point start;

    // marks
    int target;
    std::vector<bool> vMarks;
    
    // callback
    void ActionCallback(Action const &action);

    // topology
    void MarkTfo(int id);

    // time
    bool Timeout();

    // reduce fanin
    bool ReduceFanin(int id, bool fRemoveUnused = false);
    bool ReduceFaninOneRandom(int id, bool fRemoveUnused = false);

    // reduce
    void Reduce();
    void ReduceRandom();

    // remove redundancy
    void RemoveRedundancy();
    void RemoveRedundancyRandom();

    // addition
    template <typename T>
    T SingleAdd(int id, T begin, T end);
    int  MultiAdd(int id, std::vector<int> const &vCands, int nMax = 0);

    // resub
    void SingleResub(bool fGreedy = true);
    void SingleResubRandom(int nItes, int nAdds, bool fGreedy = true);
    void MultiResub(bool fGreedy = true, int nMax = 0);
    //void SingleReplace();
    
  public:
    // constructors
    Optimizer(Ntk *pNtk, Parameter *pPar);
    ~Optimizer();
    void UpdateNetwork(Ntk *pNtk_);

    // run
    void Run(seconds nTimeout_ = 0);
  };

  /* {{{ Callback */
  
  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::ActionCallback(Action const &action) {
    if(nVerbose) {
      PrintAction(action);
    }
    switch(action.type) {
    case REMOVE_FANIN:
      if(action.id != target) {
        target = -1;
      }
      break;
    case REMOVE_UNUSED:
      break;
    case REMOVE_BUFFER:
    case REMOVE_CONST:
      if(action.id == target) {
        target = -1;
      }
      break;
    case ADD_FANIN:
      if(action.id != target) {
        target = -1;
      }
      break;
    case TRIVIAL_COLLAPSE:
      break;
    case TRIVIAL_DECOMPOSE:
      target = -1;
      break;
    case SAVE:
      break;
    case LOAD:
      //target = -1; // this is not always needed, so do it manually
      break;
    case POP_BACK:
      break;
    default:
      assert(0);
    }
  }

  /* }}} */

  /* {{{ Topology */
  
  template <typename Ntk, typename Ana>
  inline void Optimizer<Ntk, Ana>::MarkTfo(int id) {
    // includes id itself
    if(id == target) {
      return;
    }
    target = id;
    vMarks.clear();
    vMarks.resize(pNtk->GetNumNodes());
    vMarks[id] = true;
    pNtk->ForEachTfo(id, false, [&](int fo) {
      vMarks[fo] = true;
    });
  }

  /* }}} */

  /* {{{ Time */
  
  template <typename Ntk, typename Ana>
  inline bool Optimizer<Ntk, Ana>::Timeout() {
    if(nTimeout) {
      time_point current = GetCurrentTime();
      if(DurationInSeconds(start, current) > nTimeout) {
        return true;
      }
    }
    return false;
  }

  /* }}} */

  /* {{{ Reduce fanin */
  
  template <typename Ntk, typename Ana>
  inline bool Optimizer<Ntk, Ana>::ReduceFanin(int id, bool fRemoveUnused) {
    assert(pNtk->GetNumFanouts(id) > 0);
    bool fRemoved = false;
    for(int idx = 0; idx < pNtk->GetNumFanins(id); idx++) {
      // skip fanins that were just added
      if(mapNewFanins.count(id)) {
        int fi = pNtk->GetFanin(id, idx);
        if(mapNewFanins[id].count(fi)) {
          continue;
        }
      }
      // reduce
      if(pAna->CheckRedundancy(id, idx)) {
        int fi = pNtk->GetFanin(id, idx);
        pNtk->RemoveFanin(id, idx);
        fRemoved = true;
        idx--;
        if(fRemoveUnused && pNtk->GetNumFanouts(fi) == 0) {
          pNtk->RemoveUnused(fi, true);
        }
      }
    }
    return fRemoved;
  }

  template <typename Ntk, typename Ana>
  inline bool Optimizer<Ntk, Ana>::ReduceFaninOneRandom(int id, bool fRemoveUnused) {
    assert(pNtk->GetNumFanouts(id) > 0);
    // generate random order
    vTmp.resize(pNtk->GetNumFanins(id));
    std::iota(vTmp.begin(), vTmp.end(), 0);
    std::shuffle(vTmp.begin(), vTmp.end(), rng);
    for(int idx: vTmp) {
      // skip fanins that were just added
      if(mapNewFanins.count(id)) {
        int fi = pNtk->GetFanin(id, idx);
        if(mapNewFanins[id].count(fi)) {
          continue;
        }
      }
      // reduce
      if(pAna->CheckRedundancy(id, idx)) {
        int fi = pNtk->GetFanin(id, idx);
        pNtk->RemoveFanin(id, idx);
        if(fRemoveUnused && pNtk->GetNumFanouts(fi) == 0) {
          pNtk->RemoveUnused(fi, true);
        }
        return true;
      }
    }
    return false;
  }
  
  // TODO: add a method to minimize the size of fanins (check singles, pairs, trios, and so on)?
  
  /* }}} */
  
  /* {{{ Reduce */

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::Reduce() {
    std::vector<int> vInts = pNtk->GetInts();
    for(critr it = vInts.rbegin(); it != vInts.rend(); it++) {
      if(!pNtk->IsInt(*it)) {
        continue;
      }
      if(pNtk->GetNumFanouts(*it) == 0) {
        pNtk->RemoveUnused(*it);
        continue;
      }
      ReduceFanin(*it);
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
      }
      /*
      if(pNtk->GetNumFanins(*it) == 1) {
        pNtk->RemoveBuffer(*it);
      }
      if(pNtk->GetNumFanins(*it) == 0) {
        pNtk->RemoveConst(*it);
      }
      */
    }
  }

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::ReduceRandom() {
    pNtk->Sweep(false);
    std::vector<int> vInts = pNtk->GetInts();
    std::shuffle(vInts.begin(), vInts.end(), rng);
    for(citr it = vInts.begin(); it != vInts.end(); it++) {
      if(!pNtk->IsInt(*it)) {
        continue;
      }
      if(pNtk->GetNumFanouts(*it) == 0) {
        pNtk->RemoveUnused(*it);
        continue;
      }
      ReduceFanin(*it, true);
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
      }
    }
  }

  /* }}} */

  /* {{{ Redundancy removal */
  
  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::RemoveRedundancy() {
    std::vector<int> vInts = pNtk->GetInts();
    for(critr it = vInts.rbegin(); it != vInts.rend();) {
      if(!pNtk->IsInt(*it)) {
        it++;
        continue;
      }
      if(pNtk->GetNumFanouts(*it) == 0) {
        pNtk->RemoveUnused(*it);
        it++;
        continue;
      }
      bool fReduced = ReduceFanin(*it);
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
      }
      if(fReduced) {
        it = vInts.rbegin();
      } else {
        it++;
      }
    }
  }

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::RemoveRedundancyRandom() {
    pNtk->Sweep(false);
    std::vector<int> vInts = pNtk->GetInts();
    std::shuffle(vInts.begin(), vInts.end(), rng);
    for(citr it = vInts.begin(); it != vInts.end();) {
      if(!pNtk->IsInt(*it)) {
        it++;
        continue;
      }
      if(pNtk->GetNumFanouts(*it) == 0) {
        pNtk->RemoveUnused(*it);
        it++;
        continue;
      }
      bool fReduced = ReduceFaninOneRandom(*it, true);
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
      }
      if(fReduced) {
        std::shuffle(vInts.begin(), vInts.end(), rng);
        it = vInts.begin();
      } else {
        it++;
      }
    }
  }

  /* }}} */

  /* {{{ Addition */

  template <typename Ntk, typename Ana>
  template <typename T>
  T Optimizer<Ntk, Ana>::SingleAdd(int id, T begin, T end) {
    MarkTfo(id);
    pNtk->ForEachFanin(id, [&](int fi, bool c) {
      vMarks[fi] = true;
    });
    T it = begin;
    for(; it != end; it++) {
      if(!pNtk->IsInt(*it) && !pNtk->IsPi(*it)) {
        continue;
      }
      if(vMarks[*it]) {
        continue;
      }
      if(pAna->CheckFeasibility(id, *it, false)) {
        pNtk->AddFanin(id, *it, false);
      } else if(pNtk->UseComplementedEdges() && pAna->CheckFeasibility(id, *it, true)) {
        pNtk->AddFanin(id, *it, true);
      } else {
        continue;
      }
      mapNewFanins[id].insert(*it);
      break;
    }
    pNtk->ForEachFanin(id, [&](int fi, bool c) {
      vMarks[fi] = false;
    });
    return it;
  }

  template <typename Ntk, typename Ana>
  int Optimizer<Ntk, Ana>::MultiAdd(int id, std::vector<int> const &vCands, int nMax) {
    MarkTfo(id);
    pNtk->ForEachFanin(id, [&](int fi, bool c) {
      vMarks[fi] = true;
    });
    int nAdded = 0;
    for(int cand: vCands) {
      if(!pNtk->IsInt(cand) && !pNtk->IsPi(cand)) {
        continue;
      }
      if(vMarks[cand]) {
        continue;
      }
      if(pAna->CheckFeasibility(id, cand, false)) {
        pNtk->AddFanin(id, cand, false);
      } else if(pNtk->UseComplementedEdges() && pAna->CheckFeasibility(id, cand, true)) {
        pNtk->AddFanin(id, cand, true);
      } else {
        continue;
      }
      mapNewFanins[id].insert(cand);
      nAdded++;
      if(nAdded == nMax) {
        break;
      }
    }
    pNtk->ForEachFanin(id, [&](int fi, bool c) {
      vMarks[fi] = false;
    });
    return nAdded;
  }

  /* }}} */
  
  /* {{{ Constructors */
  
  template <typename Ntk, typename Ana>
  Optimizer<Ntk, Ana>::Optimizer(Ntk *pNtk, Parameter *pPar) :
    pNtk(pNtk),
    nVerbose(pPar->nOptimizerVerbose),
    target(-1) {
    CostFunction = [&](Ntk *pNtk) {
      int nTwoInputSize = 0;
      pNtk->ForEachInt([&](int id) {
        nTwoInputSize += pNtk->GetNumFanins(id) - 1;
      });
      return nTwoInputSize;
    };
    pNtk->AddCallback(std::bind(&Optimizer<Ntk, Ana>::ActionCallback, this, std::placeholders::_1));
    pAna = new Ana(pNtk, pPar);
    rng.seed(pPar->iSeed);
  }
  
  template <typename Ntk, typename Ana>
  Optimizer<Ntk, Ana>::~Optimizer() {
    delete pAna;
  }
  
  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::UpdateNetwork(Ntk *pNtk_) {
    pNtk = pNtk_;
    target = -1;
    pAna->UpdateNetwork(pNtk);
  }
  
  /* }}} */

  /* {{{ Resub */

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::SingleResub(bool fGreedy) {
    // save if wanted
    int slot = fGreedy? pNtk->Save(): -1;
    double dCost = CostFunction(pNtk);
    // main loop
    std::vector<int> vInts = pNtk->GetInts();
    for(critr it = vInts.rbegin(); it != vInts.rend(); it++) {
      if(Timeout()) {
        break;
      }
      if(!pNtk->IsInt(*it)) {
        continue;
      }
      if(nVerbose) {
        std::cout << "node " << *it << " (" << std::distance(vInts.crbegin(), it) + 1 << "/" << vInts.size() << ")" << std::endl;
      }
      assert(pNtk->GetNumFanouts(*it) != 0);
      assert(pNtk->GetNumFanins(*it) > 1);
      pNtk->TrivialCollapse(*it);
      std::vector<int> vCands = pNtk->GetPis();
      std::vector<int> vInts2 = pNtk->GetInts();
      vCands.insert(vCands.end(), vInts2.begin(), vInts2.end());
      citr it2 = vCands.begin();
      while(true) {
        if(Timeout()) {
          break;
        }
        it2 = SingleAdd<citr>(*it, it2, vCands.end());
        if(it2 == vCands.end()) {
          break;
        }
        RemoveRedundancy();
        mapNewFanins.clear();
        if(!pNtk->IsInt(*it)) {
          dCost = CostFunction(pNtk);
          if(nVerbose) {
            std::cout << "new cost = " << dCost << std::endl;
          }
          if(fGreedy) {
            pNtk->Save(slot);
          }
          break;
        }
        double dNewCost = CostFunction(pNtk);
        if(fGreedy) {
          if(nVerbose) {
            std::cout << "new cost = " << dNewCost << std::endl;
          }
          if(dNewCost <= dCost) {
            pNtk->Save(slot);
            dCost = dNewCost;
          } else {
            pNtk->Load(slot);
          }
        } else {
          dCost = dNewCost;
        }
        it2++;
      }
      if(pNtk->IsInt(*it)) {
        pNtk->TrivialDecompose(*it);
      }
    }
    if(fGreedy) {
      pNtk->PopBack();
    }
  }

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::SingleResubRandom(int nItes, int nAdds, bool fGreedy) {
    // save if wanted
    int slot = fGreedy? pNtk->Save(): -1;
    double dCost = CostFunction(pNtk);
    // main loop
    for(int i = 0; i < nItes; i++) {
      if(nVerbose) {
        std::cout << "ite " << i << " (cost = " << dCost << ")" << std::endl;
      }
      int nAdded = 0;
      for(int j = 0; j < nAdds; j++) {
        // prepare
        std::vector<int> vInts = pNtk->GetInts();
        std::shuffle(vInts.begin(), vInts.end(), rng);
        std::vector<int> vCands = pNtk->GetPis();
        vCands.insert(vCands.end(), vInts.begin(), vInts.end());
        std::shuffle(vCands.begin(), vCands.end(), rng);
        // add
        // (here, reduce guarantees node is used. it's not easy to check if node is used in general though, because some nodes remain alive when it has unused parents.)
        bool fAdded = false;
        for(int id: vInts) {
          if(SingleAdd(id, vCands.begin(), vCands.end()) != vCands.end()) {
            nAdded++;
            fAdded = true;
            break;
          }
        }
        if(!fAdded) {
          break;
        }
      }
      if(!nAdded) {
        break;
      }
      // reduce
      RemoveRedundancyRandom();
      // cost and save/load if wanted
      double dNewCost = CostFunction(pNtk);
      if(fGreedy) {
        if(nVerbose) {
          std::cout << "new cost = " << dNewCost << std::endl;
        }
        if(dNewCost <= dCost) {
          pNtk->Save(slot);
          dCost = dNewCost;
        } else {
          pNtk->Load(slot);
        }
      } else {
        dCost = dNewCost;
      }
    }
    if(fGreedy) {
      pNtk->PopBack();
    }
  }

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::MultiResub(bool fGreedy, int nMax) {
    // save if wanted
    int slot = fGreedy? pNtk->Save(): -1;
    double dCost = CostFunction(pNtk);
    // main loop
    std::vector<int> vInts = pNtk->GetInts();
    for(critr it = vInts.rbegin(); it != vInts.rend(); it++) {
      if(Timeout()) {
        break;
      }
      if(nVerbose) {
        std::cout << "node " << *it << " (" << std::distance(vInts.crbegin(), it) + 1 << "/" << vInts.size() << ")" << std::endl;
      }
      if(!pNtk->IsInt(*it)) {
        continue;
      }
      assert(pNtk->GetNumFanouts(*it) != 0);
      assert(pNtk->GetNumFanins(*it) > 1);
      pNtk->TrivialCollapse(*it);
      std::vector<int> vCands = pNtk->GetPis();
      std::vector<int> vInts2 = pNtk->GetInts();
      vCands.insert(vCands.end(), vInts2.begin(), vInts2.end());
      MultiAdd(*it, vCands, nMax);
      RemoveRedundancy();
      mapNewFanins.clear();
      RemoveRedundancy();
      double dNewCost = CostFunction(pNtk);
      if(fGreedy) {
        if(nVerbose) {
          std::cout << "new cost = " << dNewCost << std::endl;
        }
        if(dNewCost <= dCost) {
          pNtk->Save(slot);
          dCost = dNewCost;
        } else {
          pNtk->Load(slot);
        }
      }
      if(pNtk->IsInt(*it)) {
        pNtk->TrivialDecompose(*it);
      }
    }
    if(fGreedy) {
      pNtk->PopBack();
    }
  }

  /*
  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::SingleReplace() {
    std::vector<int> vInts = pNtk->GetInts();
    for(critr it = vInts.rbegin(); it != vInts.rend(); it++) {
      if(nVerbose) {
        std::cout << "node " << *it << " (" << std::distance(vInts.crbegin(), it) + 1 << "/" << vInts.size() << ")" << std::endl;
      }
      if(!pNtk->IsInt(*it)) {
        continue;
      }
      if(pNtk->GetNumFanouts(*it) == 0) {
        pNtk->RemoveUnused(*it);
        continue;
      }
      ReduceFanin(*it);
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
        continue;
      }
      vMarks.clear();
      vMarks.resize(pNtk->GetNumNodes());
      vMarks[*it] = true;
      pNtk->ForEachTfo(*it, false, [&](int fo) {
        vMarks[fo] = true;
      });
      pNtk->ForEachFanin(*it, [&](int fi, bool c) {
        vMarks[fi] = true;
      });
      for(citr it2 = vInts.begin(); it2 != vInts.end(); it2++) {
        if(!pNtk->IsInt(*it)) {
          break;
        }
        if(!pNtk->IsInt(*it2)) {
          continue;
        }
        if(vMarks[*it2]) {
          continue;
        }
        if(pAna->CheckFeasibility(*it, *it2, false)) {
          pNtk->AddFanin(*it, *it2, false);
        } else if(pNtk->UseComplementedEdges() && pAna->CheckFeasibility(*it, *it2, true)) {
          pNtk->AddFanin(*it, *it2, true);
        } else {
          continue;
        }
        ReduceFanin(*it);
        if(pNtk->GetNumFanins(*it) <= 1) {
          break;
        }
      }
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
        continue;
      }
    }
  }
  */

  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::Run(uint64_t nTimeout_) {
    nTimeout = nTimeout_;
    start = GetCurrentTime();
    RemoveRedundancy();
    //opt.RemoveRedundancyRandom();
    //opt.Reduce();
    //opt.ReduceRandom();
    //opt.SingleResubRandom(10, 5);
    SingleResub();
    MultiResub();
  }

  /* }}} */
  
}
