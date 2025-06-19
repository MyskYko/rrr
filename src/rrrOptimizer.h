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
  class Optimizer {
  private:
    static constexpr int nReductionMethod = 1;
    static constexpr int nSamples = 1000;
    
    // aliases
    using itr = std::vector<int>::iterator;
    using citr = std::vector<int>::const_iterator;
    using ritr = std::vector<int>::reverse_iterator;
    using critr = std::vector<int>::const_reverse_iterator;
    static constexpr bool fFastRr = true;
    
    // pointer to network
    Ntk *pNtk;
    
    // parameters
    int nVerbose;
    std::function<double(Ntk *)> CostFunction;
    int nSortTypeOriginal;
    int nSortType;
    int nFlow;
    int nDistance;
    bool fCompatible;
    bool fGreedy;
    std::string strTemporary;
    seconds nTimeout; // assigned upon Run
    std::function<void(std::string)> PrintLine;

    int nTargets = 3;

    // data
    Ana ana;
    std::mt19937 rng;
    std::vector<int> vTmp;
    std::map<int, std::set<int>> mapNewFanins;
    time_point start;

    // fanin sorting data
    std::vector<int> vRandPiOrder;
    std::vector<double> vRandCosts;

    // marks
    int target;
    std::vector<bool> vTfoMarks;

    // statistics
    struct Stats;
    std::map<std::string, Stats> stats;
    Stats statsLocal;

    // print
    template<typename... Args>
    void Print(int nVerboseLevel, Args... args);
    
    // callback
    void ActionCallback(Action const &action);

    // topology
    void MarkTfo(int id);

    // time
    bool Timeout();

    // sort fanins
    void SetRandPiOrder();
    void SetRandCosts();
    void SortFanins(int id);
    void SortFanins();

    // remove fanins
    bool RemoveRedundantFanins(int id, bool fRemoveUnused = false);
    bool RemoveRedundantFaninsRandom(int id, bool fRemoveUnused = false);
    bool RemoveRedundantFaninsOne(int id, bool fRemoveUnused = false);
    bool RemoveRedundantFaninsOneRandom(int id, bool fRemoveUnused = false);

    // remove traversal
    bool RemoveRedundancyOneTraversal(bool fSubRoutine = false);
    void ReduceRandom();

    // remove redundancy
    bool RemoveRedundancy();
    bool RemoveRedundancyRandom();

    // reduce
    bool Reduce();

    // addition
    template <typename T>
    T SingleAdd(int id, T begin, T end);
    int  MultiAdd(int id, std::vector<int> const &vCands, int nMax = 0);
    void Undo();

    // resub
    void SingleResub(int id, std::vector<int> const &vCands);
    void MultiResub(int id, std::vector<int> const &vCands, int nMax = 0);

    // resub stop
    bool SingleResubStop(int id, std::vector<int> const &vCands);
    bool MultiResubStop(int id, std::vector<int> const &vCands, int nMax = 0);

    // multi-target resub
    bool MultiTargetResub(std::vector<int> vTargets, int nMax = 0);
    
    // apply
    void ApplyReverseTopologically(std::function<void(int)> const &func);
    void ApplyRandomlyStop(std::function<bool(int)> const &func);
    void ApplyCombinationRandomlyStop(int k, std::function<bool(std::vector<int> const &)> const &func);
    void ApplyCombinationSampledStop(int k, int n, std::function<bool(std::vector<int> const &)> const &func);
    void ApplyMultisetSampledStop(int k, int n, std::function<bool(std::vector<int> const &)> const &func);
    
  public:
    // constructors
    Optimizer(Parameter const *pPar, std::function<double(Ntk *)> CostFunction);
    void AssignNetwork(Ntk *pNtk_, bool fReuse = false);
    void SetPrintLine(std::function<void(std::string)> const &PrintLine_);

    // run
    void Run(int iSeed = 0, seconds nTimeout_ = 0);
    void SetNumTargets(int nTargets_);

    // summary
    void ResetSummary();
    summary<int> GetStatsSummary() const;
    summary<double> GetTimesSummary() const;
  };

  /* {{{ Stats */
  
  template <typename Ntk, typename Ana>
  struct Optimizer<Ntk, Ana>::Stats {
    int nTriedFis = 0;
    int nAddedFis = 0;
    int nTried = 0;
    int nAdded = 0;
    int nChanged = 0;
    int nUps = 0;
    int nEqs = 0;
    int nDowns = 0;
    double durationAdd = 0;
    double durationReduce = 0;

    void Reset() {
      nTriedFis = 0;
      nAddedFis = 0;
      nTried = 0;
      nAdded = 0;
      nChanged = 0;
      nUps = 0;
      nEqs = 0;
      nDowns = 0;
      durationAdd = 0;
      durationReduce = 0;
    }

    Stats& operator+=(Stats const &other) {
      this->nTriedFis += other.nTriedFis;
      this->nAddedFis += other.nAddedFis;
      this->nTried    += other.nTried;
      this->nAdded    += other.nAdded;
      this->nChanged  += other.nChanged;
      this->nUps      += other.nUps;
      this->nEqs      += other.nEqs;
      this->nDowns    += other.nDowns;
      this->durationAdd    += other.durationAdd;
      this->durationReduce += other.durationReduce;
      return *this;
    }

    std::string GetString() const {
      std::stringstream ss;
      PrintNext(ss, "tried node/fanin", "=", nTried, "/", nTriedFis, ",", "added node/fanin", "=", nAdded, "/", nAddedFis, ",", "changed", "=", nChanged, ",", "up/eq/dn", "=", nUps, "/", nEqs, "/", nDowns);
      return ss.str();
    }
  };
  
  /* }}} */

  /* {{{ Print */

  template <typename Ntk, typename Ana>
  template <typename... Args>
  inline void Optimizer<Ntk, Ana>::Print(int nVerboseLevel, Args... args) {
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

  /* {{{ Callback */
  
  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::ActionCallback(Action const &action) {
    if(nVerbose > 4) {
      std::stringstream ss = GetActionDescription(action);
      std::string str;
      std::getline(ss, str);
      Print(4, str);
      while(std::getline(ss, str)) {
        Print(5, str);
      }
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
    case SORT_FANINS:
      break;
    case READ:
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
    vTfoMarks.clear();
    vTfoMarks.resize(pNtk->GetNumNodes());
    vTfoMarks[id] = true;
    pNtk->ForEachTfo(id, false, [&](int fo) {
      vTfoMarks[fo] = true;
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

  /* {{{ Sort fanins */

  template <typename Ntk, typename Ana>
  inline void Optimizer<Ntk, Ana>::SetRandPiOrder() {
    if(int_size(vRandPiOrder) != pNtk->GetNumPis()) {
      vRandPiOrder.clear();
      vRandPiOrder.resize(pNtk->GetNumPis());
      std::iota(vRandPiOrder.begin(), vRandPiOrder.end(), 0);
      std::shuffle(vRandPiOrder.begin(), vRandPiOrder.end(), rng);      
    }    
  }

  template <typename Ntk, typename Ana>
  inline void Optimizer<Ntk, Ana>::SetRandCosts() {
    std::uniform_real_distribution<> dis(std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max());
    while(int_size(vRandCosts) < pNtk->GetNumNodes()) {
      vRandCosts.push_back(dis(rng));
    }
  }

  template <typename Ntk, typename Ana>
  inline void Optimizer<Ntk, Ana>::SortFanins(int id) {
    switch(nSortType) {
    case 0: // no sorting
      break;
    case 1: // prioritize internals
      pNtk->SortFanins(id, [&](int i, int j) {
        return !pNtk->IsPi(i) && pNtk->IsPi(j);
      });
      break;
    case 2: // prioritize internals with (reversely) sorted PIs
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return pNtk->GetPiIndex(i) > pNtk->GetPiIndex(j);
        }
        return pNtk->IsPi(j);
      });
      break;
    case 3: // prioritize internals with random PI order
      SetRandPiOrder();
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return vRandPiOrder[pNtk->GetPiIndex(i)] > vRandPiOrder[pNtk->GetPiIndex(j)];
        }
        return pNtk->IsPi(j);
      });
      break;
    case 4: // smaller fanout takes larger cost
      pNtk->SortFanins(id, [&](int i, int j) {
        return pNtk->GetNumFanouts(i) < pNtk->GetNumFanouts(j);
      });
      break;
    case 5: // fanout + PI
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && !pNtk->IsPi(j)) {
          return false;
        }
        if(!pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return true;
        }
        return pNtk->GetNumFanouts(i) < pNtk->GetNumFanouts(j);
      });
      break;
    case 6: // fanout + sorted PI
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return pNtk->GetPiIndex(i) > pNtk->GetPiIndex(j);
        }
        if(pNtk->IsPi(i)) {
          return false;
        }
        if(pNtk->IsPi(j)) {
          return true;
        }
        return pNtk->GetNumFanouts(i) < pNtk->GetNumFanouts(j);
      });
      break;
    case 7: // fanout + random PI
      SetRandPiOrder();
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return vRandPiOrder[pNtk->GetPiIndex(i)] > vRandPiOrder[pNtk->GetPiIndex(j)];
        }
        if(pNtk->IsPi(i)) {
          return false;
        }
        if(pNtk->IsPi(j)) {
          return true;
        }
        return pNtk->GetNumFanouts(i) < pNtk->GetNumFanouts(j);
      });
      break;
    case 8: // reverse topological order + PI
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return false;
        }
        if(pNtk->IsPi(i)) {
          return false;
        }
        if(pNtk->IsPi(j)) {
          return true;
        }
        return pNtk->GetIntIndex(i) > pNtk->GetIntIndex(j);
      });
      break;
    case 9: // reverse topological order + sorted PI
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return pNtk->GetPiIndex(i) > pNtk->GetPiIndex(j);
        }
        if(pNtk->IsPi(i)) {
          return false;
        }
        if(pNtk->IsPi(j)) {
          return true;
        }
        return pNtk->GetIntIndex(i) > pNtk->GetIntIndex(j);
      });
      break;
    case 10: // reverse topological order + random PI
      SetRandPiOrder();
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return vRandPiOrder[pNtk->GetPiIndex(i)] > vRandPiOrder[pNtk->GetPiIndex(j)];
        }
        if(pNtk->IsPi(i)) {
          return false;
        }
        if(pNtk->IsPi(j)) {
          return true;
        }
        return pNtk->GetIntIndex(i) > pNtk->GetIntIndex(j);
      });
      break;
    case 11: // topo + fanout + PI
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && !pNtk->IsPi(j)) {
          return false;
        }
        if(!pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return true;
        }
        if(pNtk->GetNumFanouts(i) > pNtk->GetNumFanouts(j)) {
          return false;
        }
        if(pNtk->GetNumFanouts(i) < pNtk->GetNumFanouts(j)) {
          return true;
        }
        if(pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return false;
        }
        return pNtk->GetIntIndex(i) > pNtk->GetIntIndex(j);
      });
      break;
    case 12: // topo + fanout + sorted PI
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return pNtk->GetPiIndex(i) > pNtk->GetPiIndex(j);
        }
        if(pNtk->IsPi(i)) {
          return false;
        }
        if(pNtk->IsPi(j)) {
          return true;
        }
        if(pNtk->GetNumFanouts(i) > pNtk->GetNumFanouts(j)) {
          return false;
        }
        if(pNtk->GetNumFanouts(i) < pNtk->GetNumFanouts(j)) {
          return true;
        }
        return pNtk->GetIntIndex(i) > pNtk->GetIntIndex(j);
      });
      break;
    case 13: // topo + fanout + random PI
      SetRandPiOrder();
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return vRandPiOrder[pNtk->GetPiIndex(i)] > vRandPiOrder[pNtk->GetPiIndex(j)];
        }
        if(pNtk->IsPi(i)) {
          return false;
        }
        if(pNtk->IsPi(j)) {
          return true;
        }
        if(pNtk->GetNumFanouts(i) > pNtk->GetNumFanouts(j)) {
          return false;
        }
        if(pNtk->GetNumFanouts(i) < pNtk->GetNumFanouts(j)) {
          return true;
        }
        return pNtk->GetIntIndex(i) > pNtk->GetIntIndex(j);
      });
      break;
    case 14: // random order
      SetRandCosts();
      pNtk->SortFanins(id, [&](int i, int j) {
        return vRandCosts[i] > vRandCosts[j];
      });
      break;
    case 15: // random + PI
      SetRandCosts();
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && !pNtk->IsPi(j)) {
          return false;
        }
        if(!pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return true;
        }
        return vRandCosts[i] > vRandCosts[j];
      });
      break;
    case 16: // random + fanout
      SetRandCosts();
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->GetNumFanouts(i) > pNtk->GetNumFanouts(j)) {
          return false;
        }
        if(pNtk->GetNumFanouts(i) < pNtk->GetNumFanouts(j)) {
          return true;
        }
        return vRandCosts[i] > vRandCosts[j];
      });
      break;
    case 17: // random + fanout + PI
      SetRandCosts();
      pNtk->SortFanins(id, [&](int i, int j) {
        if(pNtk->IsPi(i) && !pNtk->IsPi(j)) {
          return false;
        }
        if(!pNtk->IsPi(i) && pNtk->IsPi(j)) {
          return true;
        }
        if(pNtk->GetNumFanouts(i) > pNtk->GetNumFanouts(j)) {
          return false;
        }
        if(pNtk->GetNumFanouts(i) < pNtk->GetNumFanouts(j)) {
          return true;
        }
        return vRandCosts[i] > vRandCosts[j];
      });
      break;
    default:
      assert(0);
    }
  }
  
  template <typename Ntk, typename Ana>
  inline void Optimizer<Ntk, Ana>::SortFanins() {
    pNtk->ForEachInt([&](int id) {
      SortFanins(id);
    });
  }
  
  /* }}} */
  
  /* {{{ Remove fanins */
  
  template <typename Ntk, typename Ana>
  inline bool Optimizer<Ntk, Ana>::RemoveRedundantFanins(int id, bool fRemoveUnused) {
    assert(pNtk->GetNumFanouts(id) > 0);
    bool fReduced = false;
    for(int idx = 0; idx < pNtk->GetNumFanins(id); idx++) {
      // skip fanins that were just added
      if(mapNewFanins.count(id)) {
        int fi = pNtk->GetFanin(id, idx);
        if(mapNewFanins[id].count(fi)) {
          continue;
        }
      }
      // reduce
      if(ana.CheckRedundancy(id, idx)) {
        int fi = pNtk->GetFanin(id, idx);
        pNtk->RemoveFanin(id, idx);
        fReduced = true;
        idx--;
        if(fRemoveUnused && pNtk->IsInt(fi) && pNtk->GetNumFanouts(fi) == 0) {
          pNtk->RemoveUnused(fi, true);
        }
      }
    }
    return fReduced;
  }

  template <typename Ntk, typename Ana>
  inline bool Optimizer<Ntk, Ana>::RemoveRedundantFaninsRandom(int id, bool fRemoveUnused) {
    assert(pNtk->GetNumFanouts(id) > 0);
    bool fReduced = false;
    // generate random order
    vTmp.resize(pNtk->GetNumFanins(id));
    std::iota(vTmp.begin(), vTmp.end(), 0);
    std::shuffle(vTmp.begin(), vTmp.end(), rng);
    for(int i = 0; i < int_size(vTmp); i++) {
      int idx = vTmp[i];
      // skip fanins that were just added
      if(mapNewFanins.count(id)) {
        int fi = pNtk->GetFanin(id, idx);
        if(mapNewFanins[id].count(fi)) {
          continue;
        }
      }
      // reduce
      if(ana.CheckRedundancy(id, idx)) {
        int fi = pNtk->GetFanin(id, idx);
        pNtk->RemoveFanin(id, idx);
        fReduced = true;
        for(int j = i + 1; j < int_size(vTmp); j++) {
          if(vTmp[j] > idx) {
            vTmp[j]--;
          }
        }
        if(fRemoveUnused && pNtk->IsInt(fi) && pNtk->GetNumFanouts(fi) == 0) {
          pNtk->RemoveUnused(fi, true);
        }
      }
    }
    return fReduced;
  }
  
  /*
  template <typename Ntk, typename Ana>
  inline bool Optimizer<Ntk, Ana>::RemoveRedundantFaninsOneRandom(int id, bool fRemoveUnused) {
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
      if(ana.CheckRedundancy(id, idx)) {
        int fi = pNtk->GetFanin(id, idx);
        pNtk->RemoveFanin(id, idx);
        if(fRemoveUnused && pNtk->IsInt(fi) && pNtk->GetNumFanouts(fi) == 0) {
          pNtk->RemoveUnused(fi, true);
        }
        return true;
      }
    }
    return false;
  }
  */
  
  // TODO: add a method to minimize the size of fanins (check singles, pairs, trios, and so on)?
  
  /* }}} */
  
  /* {{{ Remove traversal */

  template <typename Ntk, typename Ana>
  bool Optimizer<Ntk, Ana>::RemoveRedundancyOneTraversal(bool fSubRoutine) {
    time_point timeStart = GetCurrentTime();
    bool fReduced = false;
    std::vector<int> vInts = pNtk->GetInts();
    int n = 0;
    for(critr it = vInts.rbegin(); it != vInts.rend(); it++) {
      if(!pNtk->IsInt(*it)) {
        continue;
      }
      if(pNtk->GetNumFanouts(*it) == 0) {
        pNtk->RemoveUnused(*it);
        continue;
      }
      fReduced |= RemoveRedundantFanins(*it);
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
      }
      if(!strTemporary.empty() && n++ % 1000 == 0) {
	std::string str = strTemporary + std::to_string(n / 1000) + ".aig";
	DumpAig(str, pNtk, false);
      }
    }
    if(!fSubRoutine) {
      time_point timeEnd = GetCurrentTime();
      statsLocal.durationReduce += Duration(timeStart, timeEnd);
    }
    return fReduced;
  }

  /*
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
      RemoveRedundantFanins(*it, true);
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
      }
    }
  }
  */

  /* }}} */

  /* {{{ Redundancy removal */
  
  template <typename Ntk, typename Ana>
  bool Optimizer<Ntk, Ana>::RemoveRedundancy() {
    time_point timeStart = GetCurrentTime();
    bool fReduced = false;
    if(fCompatible) {
      while(RemoveRedundancyOneTraversal(true)) {
        fReduced = true;
        SortFanins();
      }
    } else if(fFastRr) {
      bool fReduced_ = true;
      std::vector<int> vInts;
      while(fReduced_) {
        fReduced_ = false;
        vInts = pNtk->GetInts();
        for(critr it = vInts.rbegin(); it != vInts.rend(); it++) {
          if(!pNtk->IsInt(*it)) {
            continue;
          }
          if(pNtk->GetNumFanouts(*it) == 0) {
            pNtk->RemoveUnused(*it);
            continue;
          }
          SortFanins(*it);
          fReduced_ |= RemoveRedundantFanins(*it);
          if(pNtk->GetNumFanins(*it) <= 1) {
            pNtk->Propagate(*it);
          }
        }
        fReduced |= fReduced_;
      }
    } else {
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
        SortFanins(*it);
        bool fReduced_ = RemoveRedundantFanins(*it);
        // design choice: we could stop at one removal and start over
        fReduced |= fReduced_;
        if(pNtk->GetNumFanins(*it) <= 1) {
          pNtk->Propagate(*it);
        }
        if(fReduced_) {
          it = vInts.rbegin();
        } else {
          it++;
        }
      }
    }
    time_point timeEnd = GetCurrentTime();
    statsLocal.durationReduce += Duration(timeStart, timeEnd);
    return fReduced;
  }

  template <typename Ntk, typename Ana>
  bool Optimizer<Ntk, Ana>::RemoveRedundancyRandom() {
    // TODO: compatible dc?
    time_point timeStart = GetCurrentTime();
    bool fReduced = false;
    //pNtk->Sweep(false);
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
      bool fReduced_ = RemoveRedundantFaninsRandom(*it, true);
      // design choice: we could stop at one removal and start over
      fReduced |= fReduced_;
      if(pNtk->GetNumFanins(*it) <= 1) {
        pNtk->Propagate(*it);
      }
      if(fReduced_) {
        std::shuffle(vInts.begin(), vInts.end(), rng);
        it = vInts.begin();
      } else {
        it++;
      }
    }
    return fReduced;
  }

  /* }}} */

  /* {{{ Reduce */

  template <typename Ntk, typename Ana>
  bool Optimizer<Ntk, Ana>::Reduce() {
    int r;
    switch(nReductionMethod) {
    case 0:
      r = RemoveRedundancy();
      break;
    case 1:
      r = RemoveRedundancyRandom();
      break;
    default:
      assert(0);
    }
    return r;
  }

  /* }}} */

  /* {{{ Addition */

  template <typename Ntk, typename Ana>
  template <typename T>
  T Optimizer<Ntk, Ana>::SingleAdd(int id, T begin, T end) {
    time_point timeStart = GetCurrentTime();
    MarkTfo(id);
    pNtk->ForEachFanin(id, [&](int fi) {
      vTfoMarks[fi] = true;
    });
    T it = begin;
    for(; it != end; it++) {
      if(!pNtk->IsInt(*it) && !pNtk->IsPi(*it)) {
        continue;
      }
      if(vTfoMarks[*it]) {
        continue;
      }
      statsLocal.nTriedFis++;
      if(ana.CheckFeasibility(id, *it, false)) {
        pNtk->AddFanin(id, *it, false);
        statsLocal.nAddedFis++;
      } else if(pNtk->UseComplementedEdges() && ana.CheckFeasibility(id, *it, true)) {
        pNtk->AddFanin(id, *it, true);
        statsLocal.nAddedFis++;
      } else {
        continue;
      }
      mapNewFanins[id].insert(*it);
      break;
    }
    pNtk->ForEachFanin(id, [&](int fi) {
      vTfoMarks[fi] = false;
    });
    time_point timeEnd = GetCurrentTime();
    statsLocal.durationAdd += Duration(timeStart, timeEnd);
    return it;
  }

  template <typename Ntk, typename Ana>
  int Optimizer<Ntk, Ana>::MultiAdd(int id, std::vector<int> const &vCands, int nMax) {
    time_point timeStart = GetCurrentTime();
    MarkTfo(id);
    pNtk->ForEachFanin(id, [&](int fi) {
      vTfoMarks[fi] = true;
    });
    int nAddedFis_ = 0;
    for(int cand: vCands) {
      if(!pNtk->IsInt(cand) && !pNtk->IsPi(cand)) {
        continue;
      }
      if(vTfoMarks[cand]) {
        continue;
      }
      statsLocal.nTriedFis++;
      if(ana.CheckFeasibility(id, cand, false)) {
        pNtk->AddFanin(id, cand, false);
        statsLocal.nAddedFis++;
      } else if(pNtk->UseComplementedEdges() && ana.CheckFeasibility(id, cand, true)) {
        pNtk->AddFanin(id, cand, true);
        statsLocal.nAddedFis++;
      } else {
        continue;
      }
      mapNewFanins[id].insert(cand);
      nAddedFis_++;
      if(nAddedFis_ == nMax) {
        break;
      }
    }
    pNtk->ForEachFanin(id, [&](int fi) {
      vTfoMarks[fi] = false;
    });
    time_point timeEnd = GetCurrentTime();
    statsLocal.durationAdd += Duration(timeStart, timeEnd);
    return nAddedFis_;
  }

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::Undo() {
    for(auto const &entry: mapNewFanins) {
      int id = entry.first;
      for(int fi: entry.second) {
        int idx = pNtk->FindFanin(id, fi);
        pNtk->RemoveFanin(id, idx);
      }
    }
  }

  /* }}} */
  
  /* {{{ Resub */

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::SingleResub(int id, std::vector<int> const &vCands) {
    // NOTE: this assumes trivial collapse/decompose does not change cost
    // let us assume the node is not trivially redundant for now
    assert(pNtk->GetNumFanouts(id) != 0);
    assert(pNtk->GetNumFanins(id) > 1);
    // collapse
    pNtk->TrivialCollapse(id);
    // save if wanted
    int slot = -2;
    if(fGreedy || fCompatible) {
      slot = pNtk->Save();
    }
    // main loop
    double cost = CostFunction(pNtk);
    bool fTried = false, fAdded = false;
    for(citr it = vCands.begin(); it != vCands.end(); it++) {
      if(Timeout()) {
        break;
      }
      if(!pNtk->IsInt(id)) {
        break;
      }
      fTried = true;
      it = SingleAdd<citr>(id, it, vCands.end());
      if(it == vCands.end()) {
        break;
      }
      fAdded = true;
      Print(2, "cand", *it, "(", int_distance(vCands.begin(), it) + 1, "/", int_size(vCands), ")", ":", "cost", "=", cost);
      if(Reduce()) {
        double costNew = CostFunction(pNtk);
        // stats
        statsLocal.nChanged++;
        if(costNew < cost) {
          statsLocal.nDowns++;
        } else if (costNew == cost) {
          statsLocal.nEqs++;
        } else {
          statsLocal.nUps++;
        }
        // greedy
        if(fGreedy) {
          if(costNew <= cost) {
            pNtk->Save(slot);
            cost = costNew;
          } else {
            pNtk->Load(slot);
          }
        } else {
          cost = costNew;
        }
      } else {
        // assuming addition only always increases cost
        if(fCompatible) {
          pNtk->Load(slot);
        } else {
          Undo();
        }
      }
      mapNewFanins.clear();
    }
    if(pNtk->IsInt(id)) {
      pNtk->TrivialDecompose(id);
      SortFanins();
      if(fCompatible) {
        Reduce();
      }
    }
    if(fGreedy || fCompatible) {
      pNtk->PopBack();
    }
    if(fTried) {
      statsLocal.nTried++;
    }
    if(fAdded) {
      statsLocal.nAdded++;
    }
  }

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::MultiResub(int id, std::vector<int> const &vCands, int nMax) {
    // NOTE: this assumes trivial collapse/decompose does not change cost
    // let us assume the node is not trivially redundant for now
    assert(pNtk->GetNumFanouts(id) != 0);
    assert(pNtk->GetNumFanins(id) > 1);
    statsLocal.nTried++;
    // save if wanted
    int slot = -2;
    if(fGreedy || fCompatible) {
      slot = pNtk->Save();
    }
    // collapse
    pNtk->TrivialCollapse(id);
    // resub
    double cost = CostFunction(pNtk);
    if(MultiAdd(id, vCands, nMax)) {
      statsLocal.nAdded++;
      if(Reduce()) {
        mapNewFanins.clear();
        Reduce();
        double costNew = CostFunction(pNtk);
        // stats
        statsLocal.nChanged++;
        if(costNew < cost) {
          statsLocal.nDowns++;
        } else if (costNew == cost) {
          statsLocal.nEqs++;
        } else {
          statsLocal.nUps++;
        }
        // greedy
        if(fGreedy && costNew > cost) {
          pNtk->Load(slot);
        }
      } else {
        if(fCompatible) {
          pNtk->Load(slot);
        } else {
          Undo();
        }
        mapNewFanins.clear();
      }
    }
    if(pNtk->IsInt(id)) {
      pNtk->TrivialDecompose(id);
      SortFanins();
      if(fCompatible) {
        Reduce();
      }
    }
    if(fGreedy || fCompatible) {
      pNtk->PopBack();
    }
  }
  
  /* }}} */

  /* {{{ Resub stop */

  template <typename Ntk, typename Ana>
  bool Optimizer<Ntk, Ana>::SingleResubStop(int id, std::vector<int> const &vCands) {
    // stop whenever structure has changed without increasing cost and return true
    // return false if no candidates are effective
    // NOTE: this assumes trivial collapse/decompose does not change cost
    // let us assume the node is not trivially redundant for now
    assert(pNtk->GetNumFanouts(id) != 0);
    assert(pNtk->GetNumFanins(id) > 1);
    Print(2, "method", "=", "single");
    // collapse
    pNtk->TrivialCollapse(id);
    // save
    int slot = pNtk->Save();
    // remember fanins
    std::set<int> sFanins = pNtk->GetExtendedFanins(id);
    Print(3, "extended fanins", ":", sFanins);
    // main loop
    bool fChanged = false;
    double cost = CostFunction(pNtk);
    bool fTried = false, fAdded = false;
    for(citr it = vCands.begin(); it != vCands.end(); it++) {
      if(Timeout()) {
        break;
      }
      assert(pNtk->IsInt(id));
      fTried = true;
      it = SingleAdd<citr>(id, it, vCands.end());
      if(it == vCands.end()) {
        break;
      }
      fAdded = true;
      Print(3, "cand", *it, "(", int_distance(vCands.begin(), it) + 1, "/", int_size(vCands), ")", ":", "cost", "=", cost);
      if(Reduce()) {
        mapNewFanins.clear();
        double costNew = CostFunction(pNtk);
        if(costNew <= cost) {
          fChanged = false;
          if(costNew < cost || !pNtk->IsInt(id)) {
            fChanged = true;
          } else {
            std::set<int> sNewFanins = pNtk->GetExtendedFanins(id);
            Print(3, "new extended fanins", ":", sNewFanins);
            if(sFanins != sNewFanins) {
              fChanged = true;
            }
          }
          if(fChanged) {
            statsLocal.nChanged++;
            if(costNew < cost) {
              statsLocal.nDowns++;
            } else if (costNew == cost) {
              statsLocal.nEqs++;
            } else {
              statsLocal.nUps++;
            }
            break;
          }
        }
        pNtk->Load(slot);
      } else {
        if(fCompatible) {
          pNtk->Load(slot);
        } else {
          Undo();
        }
        mapNewFanins.clear();
      }
    }
    if(pNtk->IsInt(id)) {
      pNtk->TrivialDecompose(id);
    }
    pNtk->PopBack();
    if(fTried) {
      statsLocal.nTried++;
    }
    if(fAdded) {
      statsLocal.nAdded++;
    }
    return fChanged;
  }

  template <typename Ntk, typename Ana>
  bool Optimizer<Ntk, Ana>::MultiResubStop(int id, std::vector<int> const &vCands, int nMax) {
    // if structure has changed without increasing cost, return true
    // otherwise, return false
    // NOTE: this assumes trivial collapse/decompose does not change cost
    // let us assume the node is not trivially redundant for now
    assert(pNtk->GetNumFanouts(id) != 0);
    assert(pNtk->GetNumFanins(id) > 1);
    Print(2, "method", "=", "multi");
    statsLocal.nTried++;
    // save
    int slot = pNtk->Save();
    // remember fanins
    std::set<int> sFanins = pNtk->GetExtendedFanins(id);
    Print(3, "extended fanins", ":", sFanins);
    // collapse
    pNtk->TrivialCollapse(id);
    // resub
    bool fChanged = false;
    double cost = CostFunction(pNtk);
    if(MultiAdd(id, vCands, nMax)) {
      statsLocal.nAdded++;
      if(Reduce()) {
        mapNewFanins.clear();
        Reduce();
        double costNew = CostFunction(pNtk);
        if(!fGreedy || costNew <= cost) {
          fChanged = false;
          if(costNew < cost || !pNtk->IsInt(id)) {
            fChanged = true;
          } else {
            std::set<int> sNewFanins = pNtk->GetExtendedFanins(id);
            Print(3, "new extended fanins", ":", sNewFanins);
            if(sFanins != sNewFanins) {
              fChanged = true;
            }
          }
          if(fChanged) {
            statsLocal.nChanged++;
            if(costNew < cost) {
              statsLocal.nDowns++;
            } else if (costNew == cost) {
              statsLocal.nEqs++;
            } else {
              statsLocal.nUps++;
            }
          }
        }
      } else {
        mapNewFanins.clear();
      }
    }
    if(fChanged) {
      if(pNtk->IsInt(id)) {
        pNtk->TrivialDecompose(id);
      }
    } else {
      pNtk->Load(slot);
    }
    pNtk->PopBack();
    return fChanged;
  }
  
  /* }}} */

  /* {{{ Multi-target resub */

  template <typename Ntk, typename Ana>
  bool Optimizer<Ntk, Ana>::MultiTargetResub(std::vector<int> vTargets, int nMax) {
    statsLocal.nTried++;
    // save
    int slot = pNtk->Save();
    double cost = CostFunction(pNtk);
    // remove targets that are trivially collapsed together
    for(int id: vTargets) {
      if(pNtk->IsInt(id)) {
        pNtk->TrivialCollapse(id);
      }
    }
    for(itr it = vTargets.begin(); it != vTargets.end();) {
      if(!pNtk->IsInt(*it)) {
        it = vTargets.erase(it);
      } else {
        it++;
      }
    }
    // remember extended fanins
    Print(2, "targets", ":", vTargets);
    std::set<int> sTargets(vTargets.begin(), vTargets.end());
    std::map<int, std::set<int>> msFanins;
    for(int id: sTargets) {
      // remember fanins
      std::set<int> sFanins = pNtk->GetExtendedFanins(id);
      Print(3, "extended fanins", id, ":", sFanins);
      msFanins[id] = std::move(sFanins);
    }    
    // add
    bool fAdded = false;
    for(int id: vTargets) {
      // get candidates
      std::vector<int> vCands;
      if(nDistance) {
        vCands = pNtk->GetNeighbors(id, true, nDistance);
      } else {
        vCands = pNtk->GetPisInts();
      }
      std::shuffle(vCands.begin(), vCands.end(), rng);
      // add
      fAdded |= MultiAdd(id, vCands, nMax);
    }
    // reduce
    bool fChanged = false;
    if(fAdded) {
      statsLocal.nAdded++;
      if(Reduce()) {
        mapNewFanins.clear();
        Reduce();
        double costNew = CostFunction(pNtk);
        if(!fGreedy || costNew <= cost) {
          if(costNew < cost) {
            fChanged = true;
          } else {
            for(int id: sTargets) {
              if(!pNtk->IsInt(id)) {
                fChanged = true;
                break;
              }
            }
            if(!fChanged) {
              for(int id: sTargets) {
                std::set<int> sNewFanins = pNtk->GetExtendedFanins(id);
                Print(3, "new extended fanins", id, ":", sNewFanins);
                if(msFanins[id] != sNewFanins) {
                  fChanged = true;
                  break;
                }
              }
            }
          }
          if(fChanged) {
            statsLocal.nChanged++;
            if(costNew < cost) {
              statsLocal.nDowns++;
            } else if (costNew == cost) {
              statsLocal.nEqs++;
            } else {
              statsLocal.nUps++;
            }
          }
        }
      } else {
        mapNewFanins.clear();
      }
    }
    if(fChanged) {
      for(int id: sTargets) {
        if(pNtk->IsInt(id)) {
          pNtk->TrivialDecompose(id);
        }
      }
    } else {
      pNtk->Load(slot);
      target = -1;
    }
    pNtk->PopBack();
    return fChanged;
  }
    
  /* }}} */
  
  /* {{{ Apply */

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::ApplyReverseTopologically(std::function<void(int)> const &func) {
    std::vector<int> vInts = pNtk->GetInts();
    for(critr it = vInts.rbegin(); it != vInts.rend(); it++) {
      if(Timeout()) {
        break;
      }
      if(!pNtk->IsInt(*it)) {
        continue;
      }
      Print(1, "node", *it, "(", int_distance(vInts.crbegin(), it) + 1, "/", int_size(vInts), ")", ":", "cost", "=", CostFunction(pNtk));
      func(*it);
    }
  }

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::ApplyRandomlyStop(std::function<bool(int)> const &func) {
    std::vector<int> vInts = pNtk->GetInts();
    std::shuffle(vInts.begin(), vInts.end(), rng);
    for(citr it = vInts.begin(); it != vInts.end(); it++) {
      if(Timeout()) {
        break;
      }
      if(!pNtk->IsInt(*it)) {
        continue;
      }
      Print(1, "node", *it, "(", int_distance(vInts.cbegin(), it) + 1, "/", int_size(vInts), ")", ":", "cost", "=", CostFunction(pNtk));
      if(func(*it)) {
        break;
      }
    }
  }

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::ApplyCombinationRandomlyStop(int k, std::function<bool(std::vector<int> const &)> const &func) {
    std::vector<int> vInts = pNtk->GetInts();
    std::shuffle(vInts.begin(), vInts.end(), rng); // order is decided here, so it's not truely exhaustive
    int nTried_ = 0;
    int nCombs = k * (k - 1) / 2;
    ForEachCombinationStop(int_size(vInts), k, [&](std::vector<int> const &vIdxs) {
      Print(1, "comb", vIdxs, "(", ++nTried_, "/", nCombs, ")");
      assert(int_size(vIdxs) == k);
      if(Timeout()) {
        return true;
      }
      std::vector<int> vTargets(k);
      for(int i = 0; i < k; i++) {
        vTargets[i] = vInts[vIdxs[i]];
      }
      return func(vTargets);
    });
  }
  
  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::ApplyCombinationSampledStop(int k, int n, std::function<bool(std::vector<int> const &)> const &func) {
    std::vector<int> vInts = pNtk->GetInts();
    for(int i = 0; i < n; i++) {
      if(Timeout()) {
        break;
      }
      std::set<int> sIdxs;
      while(int_size(sIdxs) < k) {
        int idx = rng() % pNtk->GetNumInts();
        sIdxs.insert(idx);
      }
      std::vector<int> vIdxs(sIdxs.begin(), sIdxs.end());
      std::shuffle(vIdxs.begin(), vIdxs.end(), rng);
      Print(1, "comb", vIdxs, "(", i + 1, "/", n, ")");
      std::vector<int> vTargets(k);
      for(int i = 0; i < k; i++) {
        vTargets[i] = vInts[vIdxs[i]];
      }
      if(func(vTargets)) {
        break;
      }
    }
  }

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::ApplyMultisetSampledStop(int k, int n, std::function<bool(std::vector<int> const &)> const &func) {
    std::vector<int> vInts = pNtk->GetInts();
    for(int i = 0; i < n; i++) {
      if(Timeout()) {
        break;
      }
      std::vector<int> vIdxs;
      for(int j = 0; j < k; j++) {
        int idx = rng() % pNtk->GetNumInts();
        vIdxs.push_back(idx);
      }
      Print(1, "comb", vIdxs, "(", i + 1, "/", n, ")");
      std::vector<int> vTargets(k);
      for(int i = 0; i < k; i++) {
        vTargets[i] = vInts[vIdxs[i]];
      }
      if(func(vTargets)) {
        break;
      }
    }
  }

  /* }}} */
  
  /* {{{ Constructors */
  
  template <typename Ntk, typename Ana>
  Optimizer<Ntk, Ana>::Optimizer(Parameter const *pPar, std::function<double(Ntk *)> CostFunction) :
    pNtk(NULL),
    nVerbose(pPar->nOptimizerVerbose),
    CostFunction(CostFunction),
    nSortTypeOriginal(pPar->nSortType),
    nSortType(pPar->nSortType),
    nFlow(pPar->nOptimizerFlow),
    nDistance(pPar->nDistance),
    fCompatible(pPar->fUseBddCspf),
    fGreedy(pPar->fGreedy),
    strTemporary(pPar->strTemporary),
    ana(pPar),
    target(-1) {
  }
  
  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    pNtk = pNtk_;
    target = -1;
    pNtk->AddCallback(std::bind(&Optimizer<Ntk, Ana>::ActionCallback, this, std::placeholders::_1));
    ana.AssignNetwork(pNtk, fReuse);
  }
  
  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::SetPrintLine(std::function<void(std::string)> const &PrintLine_) {
    PrintLine = PrintLine_;
  }
  
  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::Run(int iSeed, seconds nTimeout_) {
    rng.seed(iSeed);
    vRandPiOrder.clear();
    vRandCosts.clear();
    if(nSortTypeOriginal < 0) {
      nSortType = rng() % 18;
      Print(0, "fanin cost function =", nSortType);
    }
    nTimeout = nTimeout_;
    start = GetCurrentTime();
    switch(nFlow) {
    case 0:
      Reduce();
      statsLocal.Reset();
      ApplyReverseTopologically([&](int id) {
        std::vector<int> vCands;
        if(nDistance) {
          vCands = pNtk->GetNeighbors(id, true, nDistance);
        } else {
          vCands = pNtk->GetPisInts();
        }
        SingleResub(id, vCands);
      });
      stats["single"] += statsLocal;
      Print(0, statsLocal.GetString());
      break;
    case 1:
      Reduce();
      statsLocal.Reset();
      ApplyReverseTopologically([&](int id) {
        std::vector<int> vCands;
        if(nDistance) {
          vCands = pNtk->GetNeighbors(id, true, nDistance);
        } else {
          vCands = pNtk->GetPisInts();
        }
        MultiResub(id, vCands);
      });
      stats["multi"] += statsLocal;
      Print(0, statsLocal.GetString());
      break;
    case 2: {
      Reduce();
      double dCost = CostFunction(pNtk);
      while(true) {
        statsLocal.Reset();
        ApplyReverseTopologically([&](int id) {
          std::vector<int> vCands;
          if(nDistance) {
            vCands = pNtk->GetNeighbors(id, true, nDistance);
          } else {
            vCands = pNtk->GetPisInts();
          }
          SingleResub(id, vCands);
        });
        stats["single"] += statsLocal;
        Print(0, "single", ":", "cost", "=", CostFunction(pNtk), ",", statsLocal.GetString());
        statsLocal.Reset();
        ApplyReverseTopologically([&](int id) {
          std::vector<int> vCands;
          if(nDistance) {
            vCands = pNtk->GetNeighbors(id, true, nDistance);
          } else {
            vCands = pNtk->GetPisInts();
            // vCands = pNtk->GetInts();
          }
          MultiResub(id, vCands);
        });
        stats["multi"] += statsLocal;
        Print(0, "multi ", ":", "cost", "=", CostFunction(pNtk), ",", statsLocal.GetString());
        double dNewCost = CostFunction(pNtk);
        if(dNewCost < dCost) {
          dCost = dNewCost;
        } else {
          break;
        }
      }
      break;
    }
    case 3: {
      Reduce();
      std::vector<int> vCands;
      if(!nDistance) {
        vCands = pNtk->GetPisInts();
        std::shuffle(vCands.begin(), vCands.end(), rng);
      }
      Stats statsSingle, statsMulti;
      ApplyRandomlyStop([&](int id) {
        statsLocal.Reset();
        if(nDistance) {
          vCands = pNtk->GetNeighbors(id, true, nDistance);
          std::shuffle(vCands.begin(), vCands.end(), rng);
        }
        bool fChanged;
        if(rng() & 1) {
          fChanged = SingleResubStop(id, vCands);
          statsSingle += statsLocal;
        } else {
          fChanged = MultiResubStop(id, vCands);
          statsMulti += statsLocal;
        }
        return fChanged;
      });
      stats["single"] += statsSingle;
      stats["multi"] += statsMulti;
      Print(0, "single", ":", statsSingle.GetString());
      Print(0, "multi ", ":", statsMulti.GetString());
      break;
    }
    case 4: {
      Reduce();
      statsLocal.Reset();
      ApplyMultisetSampledStop(nTargets, nSamples, [&](std::vector<int> const &vTargets) {
        return MultiTargetResub(vTargets, 1);
      });
      stats["mt"] += statsLocal;
      Print(0, statsLocal.GetString());
      break;
    }
    case 5:
      //RemoveRedundancy();
      Reduce();
      break;
    case 6:
      RemoveRedundancy();
      for(int i = 0; i < 1; i++) {
        if(!strTemporary.empty()) {
          std::string str = strTemporary + std::to_string(i) + ".aig";
          DumpAig(str, pNtk, false);
        }
        std::vector<int> vCands;
        if(!nDistance) {
          vCands = pNtk->GetPisInts();
          std::shuffle(vCands.begin(), vCands.end(), rng);
        }
        Stats statsSingle, statsMulti;
        ApplyRandomlyStop([&](int id) {
          statsLocal.Reset();
          if(nDistance) {
            vCands = pNtk->GetNeighbors(id, true, nDistance);
            std::shuffle(vCands.begin(), vCands.end(), rng);
          }
          bool fChanged;
          if(/*rng() & 1*/true) {
            fChanged = SingleResubStop(id, vCands);
            statsSingle += statsLocal;
          } else {
            fChanged = MultiResubStop(id, vCands);
            statsMulti += statsLocal;
          }
          return fChanged;
        });
        stats["single"] += statsSingle;
        stats["multi"] += statsMulti;
        Print(0, "single", ":", statsSingle.GetString());
        Print(0, "multi ", ":", statsMulti.GetString());
      }
      break;
    default:
      assert(0);
    }
  }

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::SetNumTargets(int nTargets_) {
    nTargets = nTargets_;
  }
  
  /* }}} */

  /* {{{ Summary */

  template <typename Ntk, typename Ana>
  void Optimizer<Ntk, Ana>::ResetSummary() {
    stats.clear();
    ana.ResetSummary();
  }
  
  template <typename Ntk, typename Ana>
  summary<int> Optimizer<Ntk, Ana>::GetStatsSummary() const {
    summary<int> v;
    for(auto const &entry: stats) {
      v.emplace_back("opt " + entry.first + " tried node", entry.second.nTried);
      v.emplace_back("opt " + entry.first + " tried fanin", entry.second.nTriedFis);
      v.emplace_back("opt " + entry.first + " added node", entry.second.nAdded);
      v.emplace_back("opt " + entry.first + " added fanin", entry.second.nAddedFis);
      v.emplace_back("opt " + entry.first + " changed", entry.second.nChanged);
      v.emplace_back("opt " + entry.first + " up", entry.second.nUps);
      v.emplace_back("opt " + entry.first + " eq", entry.second.nEqs);
      v.emplace_back("opt " + entry.first + " dn", entry.second.nDowns);
    }
    summary<int> v2 = ana.GetStatsSummary();
    v.insert(v.end(), v2.begin(), v2.end());
    return v;
  }

  template <typename Ntk, typename Ana>
  summary<double> Optimizer<Ntk, Ana>::GetTimesSummary() const {
    summary<double> v;
    for(auto const &entry: stats) {
      v.emplace_back("opt " + entry.first + " add", entry.second.durationAdd);
      v.emplace_back("opt " + entry.first + " reduce", entry.second.durationReduce);
    }
    summary<double> v2 = ana.GetTimesSummary();
    v.insert(v.end(), v2.begin(), v2.end());
    return v;
  }
  
  /* }}} */
  
}
