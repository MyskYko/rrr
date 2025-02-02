#pragma once

#include <vector>
#include <map>
#include <iterator>
#include <random>
#include <numeric>

#include "rrrParameter.h"


namespace rrr {

  template <typename Ntk, typename Ana>
  class Optimizer {
  private:
    using itr = std::vector<int>::iterator;
    using citr = std::vector<int>::const_iterator;
    using ritr = std::vector<int>::reverse_iterator;
    using critr = std::vector<int>::const_reverse_iterator;

    int  nVerbose;
    Ntk *pNtk;
    Ana *pAna;

    std::function<double(Ntk *)> CostFunction;
    
    std::mt19937 rng;
    std::vector<int> tmp;
    std::vector<bool> vMarks;

    std::map<int, std::set<int>> mapNewFanins;
    
    std::function<void(Action)> CreateActionCallback();

  public:
    Optimizer(Ntk *pNtk, Parameter *pPar);
    ~Optimizer();
    void UpdateNetwork(Ntk *pNtk_);

    void SetCostFunction(std::function<double(Ntk *)>);

    int ReduceFanin(int id, bool fRemoveUnused = false);
    bool ReduceFaninRandom(int id, bool fRemoveUnused = false);

    void Reduce();
    void ReduceRandom();
    
    void RemoveRedundancy();
    void RemoveRedundancyRandom();
    
    void MarkTfoAndFanin(int id);
    citr SingleAdd(int id, citr begin, citr end);
    int MultiAdd(int id, std::vector<int> const &vCands, int nMax = -1);

    void SingleResub(bool fGreedy = true);
    void SingleResubRandom(int nItes, int nAdds, bool fGreedy = true);
    
    void SingleReplace();
  };


  /* {{{ Create action callback */
  
  template <typename Ntk, typename Ana>
  std::function<void(Action)> Optimizer<Ntk, Ana>::CreateActionCallback() {
    return [&](Action action) {
      switch(action.type) {
      case REMOVE_FANIN:
        std::cout << "removed node " << action.id << " fanin " << (action.c? "!": "") << action.fi << " at index " << action.idx << std::endl;
        break;
      case REMOVE_UNUSED: {
        std::cout << "removed unused node " << action.id << " with fanin ";
        std::string delim = "";
        for(int fi: action.vFanins) {
          std::cout << delim << fi;
          delim = ", ";
        }
        std::cout << std::endl;
        break;
      }
      case REMOVE_BUFFER: {
        std::cout << "removed buffer node " << action.id << " with fanin " << (action.c? "!": "") << action.fi << " and fanout " ;
        std::string delim = "";
        for(int fo: action.vFanouts) {
          std::cout << delim << fo;
          delim = ", ";
        }
        std::cout << std::endl;
        break;
      }
      case REMOVE_CONST: {
        std::cout << "removed constant node " << action.id << " with fanin ";
        std::string delim = "";
        for(int fi: action.vFanins) {
          std::cout << delim << fi;
          delim = ", ";
        }
        std::cout << " and fanout " ;
        delim = "";
        for(int fo: action.vFanouts) {
          std::cout << delim << fo;
          delim = ", ";
        }
        std::cout << std::endl;
        break;
      }
      case ADD_FANIN:
        std::cout << "added node " << action.id << " fanin " << (action.c? "!": "") << action.fi << " at index " << action.idx << std::endl;
        break;
      case TRIVIAL_COLLAPSE: {
        std::cout << "collapsed node " << action.id << " fanin " << (action.c? "!": "") << action.fi << " with fanin ";
        std::string delim = "";
        for(int fi: action.vFanins) {
          std::cout << delim << fi;
          delim = ", ";
        }
        std::cout << std::endl;
        break;
      }
      case TRIVIAL_DECOMPOSE: {
        std::cout << "decomposed node " << action.id << " with new fanin " << (action.c? "!": "") << action.fi << " with fanin ";
        std::string delim = "";
        for(int fi: action.vFanins) {
          std::cout << delim << fi;
          delim = ", ";
        }
        std::cout << std::endl;
        break;
      }
      case SAVE:
        std::cout << "saved to slot " << action.id << std::endl;
        break;
      case LOAD:
        std::cout << "loaded from slot " << action.id << std::endl;
        break;
      case POP_BACK:
        std::cout << "deleted slot " << action.id << std::endl;
        break;
      default:
        assert(0);
      }
    };
  }

  /* }}} Create action callback end */

  /* {{{ Constructor */
  
  template <typename Ntk, typename Ana>
  Optimizer<Ntk, Ana>::Optimizer(Ntk *pNtk, Parameter *pPar) :
    nVerbose(pPar->nOptimizerVerbose),
    pNtk(pNtk) {
    pAna = new Ana(pNtk, pPar);
    if(nVerbose) {
      pNtk->AddCallback(CreateActionCallback());
    }
    CostFunction = [&](Ntk *pNtk) {
      int nTwoInputSize = 0;
      pNtk->ForEachInt([&](int id) {
        nTwoInputSize += pNtk->GetNumFanins(id) - 1;
      });
      return nTwoInputSize;
    };
    rng.seed(pPar->iSeed);
  }
  
  template <typename Ntk, typename Ana>
  Optimizer<Ntk, Ana>::~Optimizer() {
    delete pAna;
  }
  
  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::UpdateNetwork(Ntk *pNtk_) {
    pNtk = pNtk_;
    pAna->UpdateNetwork(pNtk);
  }
  
  /* }}} Constructor end */

  /* {{{ Reduce fanin */
  
  // TODO: add a method to minimize the size of fanins (check singles, pairs, trios, and so on)?
  
  template <typename Ntk, typename Ana>
  int Optimizer<Ntk, Ana>::ReduceFanin(int id, bool fRemoveUnused) {
    int nRemoved = 0;
    int nFanins = pNtk->GetNumFanins(id);
    for(int idx = 0; idx < nFanins; idx++) {
      if(mapNewFanins.count(id)) {
        int fi = pNtk->GetFanin(id, idx);
        if(mapNewFanins[id].count(fi)) {
          continue;
        }
      }
      if(pAna->CheckRedundancy(id, idx)) {
        int fi = pNtk->GetFanin(id, idx);
        pNtk->RemoveFanin(id, idx);
        nRemoved++;
        nFanins--;
        idx--;
        if(fRemoveUnused && pNtk->GetNumFanouts(fi) == 0) {
          pNtk->RemoveUnused(fi, true);
        }
      }
    }
    return nRemoved;
  }

  template <typename Ntk, typename Ana>
  bool Optimizer<Ntk, Ana>::ReduceFaninRandom(int id, bool fRemoveUnused) {
    int nFanins = pNtk->GetNumFanins(id);
    tmp.resize(nFanins);
    std::iota(tmp.begin(), tmp.end(), 0);
    std::shuffle(tmp.begin(), tmp.end(), rng);
    for(int idx: tmp) {
      if(mapNewFanins.count(id)) {
        int fi = pNtk->GetFanin(id, idx);
        if(mapNewFanins[id].count(fi)) {
          continue;
        }
      }
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
  
  /* }}} Reduce fanin end */

  /* {{{ Reduce */

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::Reduce() {
    std::vector<int> vInts = pNtk->GetInts();
    for(critr it = vInts.rbegin(); it != vInts.rend(); it++) {
      if(nVerbose > 2) {
        std::cout << "Reduce: node " << *it << std::endl;
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
    shuffle(vInts.begin(), vInts.end(), rng);
    for(citr it = vInts.begin(); it != vInts.end(); it++) {
      if(!pNtk->IsInt(*it)) {
        continue;
      }
      if(nVerbose > 2) {
        std::cout << "Reduce: node " << *it << std::endl;
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

  /* }}} Reduce end */
  
  /* {{{ Redundancy removal */
  
  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::RemoveRedundancy() {
    std::vector<int> vInts = pNtk->GetInts();
    std::reverse(vInts.begin(), vInts.end());
    itr it = vInts.begin();
    while(it != vInts.end()) {
      if(nVerbose > 1) {
        std::cout << "RR: node " << *it << std::endl;
      }
      if(pNtk->GetNumFanouts(*it) == 0) {
        pNtk->RemoveUnused(*it);
        it = vInts.erase(it);
        continue;
      }
      bool fReduced = ReduceFanin(*it) > 0;
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
        vInts = pNtk->GetInts();
        std::reverse(vInts.begin(), vInts.end());
        it = vInts.begin();
      } else if(fReduced) {
        it = vInts.begin();
      } else {
        it++;
      }
    }
  }

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::RemoveRedundancyRandom() {
    pNtk->Sweep(false);
    std::vector<int> vInts = pNtk->GetInts();
    shuffle(vInts.begin(), vInts.end(), rng);
    itr it = vInts.begin();
    while(it != vInts.end()) {
      if(!pNtk->IsInt(*it)) {
        it++;
        continue;
      }
      if(nVerbose > 1) {
        std::cout << "RR: node " << *it << std::endl;
      }
      if(pNtk->GetNumFanouts(*it) == 0) {
        pNtk->RemoveUnused(*it);
        it = vInts.erase(it);
        continue;
      }
      bool fReduced = ReduceFaninRandom(*it, true);
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
        vInts = pNtk->GetInts();
        shuffle(vInts.begin(), vInts.end(), rng);
        it = vInts.begin();
      } else if(fReduced) {
        shuffle(vInts.begin(), vInts.end(), rng);
        it = vInts.begin();
      } else {
        it++;
      }
    }
  }

  /* }}} Redundancy removal end */

  /* {{{ Addition */

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::MarkTfoAndFanin(int id) {
    vMarks.clear();
    vMarks.resize(pNtk->GetNumNodes());
    vMarks[id] = true;
    pNtk->ForEachTfo(id, false, [&](int fo) {
      vMarks[fo] = true;
    });
    pNtk->ForEachFanin(id, [&](int fi, bool c) {
      vMarks[fi] = true;
    });
  }

  template <typename Ntk, typename Ana>
  typename Optimizer<Ntk, Ana>::citr Optimizer<Ntk, Ana>::SingleAdd(int id, citr begin, citr end) {
    MarkTfoAndFanin(id);
    citr it = begin;
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
      return it;
    }
    return it;
  }

  template <typename Ntk, typename Ana>
  int Optimizer<Ntk, Ana>::MultiAdd(int id, std::vector<int> const &vCands, int nMax) {
    MarkTfoAndFanin(id);
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
    return nAdded;
  }

  /* }}} Addition end */

  /* {{{ Resub */

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::SingleResub(bool fGreedy) {
    // save if wanted
    int slot = fGreedy? pNtk->Save(): -1;
    double dCost = CostFunction(pNtk);
    // main loop
    std::vector<int> vInts = pNtk->GetInts();
    for(critr it = vInts.rbegin(); it != vInts.rend(); it++) {
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
      citr it2 = vCands.begin();
      while(true) {
        it2 = SingleAdd(*it, it2, vCands.end());
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

  /* }}} Resub end */

}
