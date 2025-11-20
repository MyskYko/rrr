#pragma once

#include <map>
#include <iterator>
#include <random>
#include <numeric>

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"
#include "io/rrrAig.h"

namespace rrr {

  // it is assumed that removing redundancy never degrades the cost in this optimizer
  template <typename Ntk, typename Ana, typename T = int>
  class QueueOptimizer {
  private:
    // entry
    struct Entry;
    
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
    int nSortTypeOriginal;
    int nSortType;
    bool fSortInitial;
    bool fSortPerNode;
    int nFlow;
    int nReductionMethod;
    int nDistance;
    bool fCompatible;
    bool fGreedy;
    std::string strTemporary;
    seconds nTimeout; // assigned upon Run
    int nModule;
    std::function<void(std::string)> PrintLine;

    int nTargets = 3;

    // data
    Ana ana;
    std::mt19937 rng;
    std::vector<Entry> q;
    time_point start;
    int nTemporary;

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

    // evaluate redundancy
    void EvaluateRedundancy();
    
  public:
    // constructors
    QueueOptimizer(Parameter const *pPar, std::function<double(Ntk *)> CostFunction);
    void AssignNetwork(Ntk *pNtk_, bool fReuse = false);
    void SetPrintLine(std::function<void(std::string)> const &PrintLine_);

    // run
    void Run(int iSeed = 0, seconds nTimeout_ = 0);
    bool RemoveNext();
    T GetCurrent();
    void SetThreshold(T tThreshold);
    void ResetThreshold();
    T GetNext(int index = 0);
    int GetNumTemporary();
    void SetNumTemporary(int nTemporary_);
    void SetNumModule(int nModule_);
    void SetBias(std::vector<std::vector<int>> const &vBias);
    std::vector<std::vector<int>> GetContribution();
    void SetNumTargets(int nTargets_);
    
    // summary
    void ResetSummary();
    summary<int> GetStatsSummary() const;
    summary<double> GetTimesSummary() const;
  };

  /* {{{ Entry */
  
  template <typename Ntk, typename Ana, typename T>
  struct QueueOptimizer<Ntk, Ana, T>::Entry {
    // data
    T t;
    int id;
    int fi;
    
    // constructor
    Entry(T t, int id, int fi) :
      t(t),
      id(id),
      fi(fi) {
    }

    bool operator<(const Entry& other) {
      return t > other.t;
    }
  };

  /* }}} */

  /* {{{ Stats */
  
  template <typename Ntk, typename Ana, typename T>
  struct QueueOptimizer<Ntk, Ana, T>::Stats {
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

  template <typename Ntk, typename Ana, typename T>
  template <typename... Args>
  inline void QueueOptimizer<Ntk, Ana, T>::Print(int nVerboseLevel, Args... args) {
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
  
  template <typename Ntk, typename Ana, typename T>
  void QueueOptimizer<Ntk, Ana, T>::ActionCallback(Action const &action) {
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
  
  template <typename Ntk, typename Ana, typename T>
  inline void QueueOptimizer<Ntk, Ana, T>::MarkTfo(int id) {
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
  
  template <typename Ntk, typename Ana, typename T>
  inline bool QueueOptimizer<Ntk, Ana, T>::Timeout() {
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

  template <typename Ntk, typename Ana, typename T>
  inline void QueueOptimizer<Ntk, Ana, T>::SetRandPiOrder() {
    if(int_size(vRandPiOrder) != pNtk->GetNumPis()) {
      vRandPiOrder.clear();
      vRandPiOrder.resize(pNtk->GetNumPis());
      std::iota(vRandPiOrder.begin(), vRandPiOrder.end(), 0);
      std::shuffle(vRandPiOrder.begin(), vRandPiOrder.end(), rng);      
    }    
  }

  template <typename Ntk, typename Ana, typename T>
  inline void QueueOptimizer<Ntk, Ana, T>::SetRandCosts() {
    std::uniform_real_distribution<> dis(std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max());
    while(int_size(vRandCosts) < pNtk->GetNumNodes()) {
      vRandCosts.push_back(dis(rng));
    }
  }

  template <typename Ntk, typename Ana, typename T>
  inline void QueueOptimizer<Ntk, Ana, T>::SortFanins(int id) {
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
  
  template <typename Ntk, typename Ana, typename T>
  inline void QueueOptimizer<Ntk, Ana, T>::SortFanins() {
    pNtk->ForEachInt([&](int id) {
      SortFanins(id);
    });
  }
  
  /* }}} */
  
  /* {{{ Remove redundancy */
  
  template <typename Ntk, typename Ana, typename T>
  void QueueOptimizer<Ntk, Ana, T>::EvaluateRedundancy() {
    time_point timeStart = GetCurrentTime();
    q.clear();
    std::vector<int> vInts = pNtk->GetInts();
    for(critr it = vInts.rbegin(); it != vInts.rend(); it++) {
      if(!pNtk->IsInt(*it)) {
        continue;
      }
      if(pNtk->GetNumFanouts(*it) == 0) {
        pNtk->RemoveUnused(*it);
        continue;
      }
      if(pNtk->TrivialCollapse(*it)) {
        pNtk->Propagate(*it);
        pNtk->Sweep();
        continue;
      }
      assert(fSortPerNode);
      SortFanins(*it);
      for(int idx = 0; idx < pNtk->GetNumFanins(*it); idx++) {
        T t = ana.EvaluateRedundancy(*it, idx);
        int fi = pNtk->GetFanin(*it, idx);
        q.emplace_back(t, *it, fi);
      }
    }
    std::sort(q.begin(), q.end());
    time_point timeEnd = GetCurrentTime();
    statsLocal.durationReduce += Duration(timeStart, timeEnd);
  }

  /* }}} */
  
  /* {{{ Constructors */
  
  template <typename Ntk, typename Ana, typename T>
  QueueOptimizer<Ntk, Ana, T>::QueueOptimizer(Parameter const *pPar, std::function<double(Ntk *)> CostFunction) :
    pNtk(NULL),
    nVerbose(pPar->nOptimizerVerbose),
    CostFunction(CostFunction),
    nSortTypeOriginal(pPar->nSortType),
    nSortType(pPar->nSortType),
    fSortInitial(pPar->fSortInitial),
    fSortPerNode(pPar->fSortPerNode),
    nFlow(pPar->nOptimizerFlow),
    nReductionMethod(pPar->nReductionMethod),
    nDistance(pPar->nDistance),
    fCompatible(pPar->fUseBddCspf),
    fGreedy(pPar->fGreedy),
    strTemporary(pPar->strTemporary),
    nModule(-1),
    ana(pPar),
    nTemporary(0),
    target(-1) {
  }
  
  template <typename Ntk, typename Ana, typename T>
  void QueueOptimizer<Ntk, Ana, T>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    pNtk = pNtk_;
    nModule = -1;
    nTemporary = 0;
    target = -1;
    pNtk->AddCallback(std::bind(&QueueOptimizer<Ntk, Ana, T>::ActionCallback, this, std::placeholders::_1));
    ana.AssignNetwork(pNtk, fReuse);
  }
  
  template <typename Ntk, typename Ana, typename T>
  void QueueOptimizer<Ntk, Ana, T>::SetPrintLine(std::function<void(std::string)> const &PrintLine_) {
    PrintLine = PrintLine_;
  }
  
  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Ana, typename T>
  void QueueOptimizer<Ntk, Ana, T>::Run(int iSeed, seconds nTimeout_) {
    rng.seed(iSeed);
    vRandPiOrder.clear();
    vRandCosts.clear();
    if(nSortTypeOriginal < 0) {
      nSortType = (rng() + 1) % 18; // default to 9
      Print(0, "fanin cost function =", nSortType);
    }
    nTimeout = nTimeout_;
    start = GetCurrentTime();
    if(fSortInitial) {
      SortFanins();
    }
    EvaluateRedundancy();
  }

  template <typename Ntk, typename Ana, typename T>
  bool QueueOptimizer<Ntk, Ana, T>::RemoveNext() {
    // assume set threshold is called already
    assert(!q.empty());
    int id = q.back().id;
    int fi = q.back().fi;
    q.pop_back();
    if(pNtk->IsInt(id)) {
      int idx = pNtk->FindFanin(id, fi);
      if(idx != -1 && ana.CheckRedundancy(id, idx)) {
        pNtk->RemoveFanin(id, idx);
        if(pNtk->IsInt(fi) && pNtk->GetNumFanouts(fi) == 0) {
          pNtk->RemoveUnused(fi, true);
        }
        if(pNtk->GetNumFanins(id) <= 1) {
          pNtk->Propagate(id);
          pNtk->Sweep();
        }
        if(nTemporary > 0 && !strTemporary.empty()) {
          Print(0, "temporary", "=", nTemporary, "threshold", "=", ana.GetThreshold(), "cost", "=", CostFunction(pNtk));
          std::string str = strTemporary;
          if(nModule != -1) {
            str += "_" + std::to_string(nModule) + "_logic";
          }
          str += "_" +  std::to_string(nTemporary) + ".aig";
          DumpAig(str, pNtk);
          nTemporary++;
        }
        return true;
      }
    }
    return false;
  }

  template <typename Ntk, typename Ana, typename T>
  T QueueOptimizer<Ntk, Ana, T>::GetCurrent() {
    return ana.GetCurrent();
  }

  template <typename Ntk, typename Ana, typename T>
  void QueueOptimizer<Ntk, Ana, T>::SetThreshold(T tThreshold) {
    ana.SetThreshold(tThreshold);
  }
  
  template <typename Ntk, typename Ana, typename T>
  T QueueOptimizer<Ntk, Ana, T>::GetNext(int index) {
    if(q.empty() || int_size(q) < index) {
      return std::numeric_limits<T>::max();
    }
    return q[int_size(q) - index - 1].t;
  }

  template <typename Ntk, typename Ana, typename T>
  int QueueOptimizer<Ntk, Ana, T>::GetNumTemporary() {
    return nTemporary;
  }

  template <typename Ntk, typename Ana, typename T>
  void QueueOptimizer<Ntk, Ana, T>::SetNumTemporary(int nTemporary_) {
    nTemporary = nTemporary_;
  }

  template <typename Ntk, typename Ana, typename T>
  void QueueOptimizer<Ntk, Ana, T>::SetNumModule(int nModule_) {
    nModule = nModule_;
  }

  template <typename Ntk, typename Ana, typename T>
  void QueueOptimizer<Ntk, Ana, T>::SetBias(std::vector<std::vector<int>> const &vBias) {
    ana.SetBias(vBias);
  }

  template <typename Ntk, typename Ana, typename T>
  std::vector<std::vector<int>> QueueOptimizer<Ntk, Ana, T>::GetContribution() {
    return ana.GetContribution();
  }

  template <typename Ntk, typename Ana, typename T>
  void QueueOptimizer<Ntk, Ana, T>::SetNumTargets(int nTargets_) {
    nTargets = nTargets_;
  }
  
  /* }}} */

  /* {{{ Summary */

  template <typename Ntk, typename Ana, typename T>
  void QueueOptimizer<Ntk, Ana, T>::ResetSummary() {
    stats.clear();
    ana.ResetSummary();
  }
  
  template <typename Ntk, typename Ana, typename T>
  summary<int> QueueOptimizer<Ntk, Ana, T>::GetStatsSummary() const {
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

  template <typename Ntk, typename Ana, typename T>
  summary<double> QueueOptimizer<Ntk, Ana, T>::GetTimesSummary() const {
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
