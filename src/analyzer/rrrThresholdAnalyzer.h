#pragma once

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"

namespace rrr {

  template <typename T>
  concept HasDrop = requires(T t, int x) {
    { t.Drop(x) };
  };

  template <typename Ntk, typename Sim, typename T, bool fAscending>
  class ThresholdAnalyzer {
  private:
    // pointer to network
    Ntk *pNtk;

    // parameters
    bool nVerbose;
    T tThreshold;
    T tNext;
    std::pair<int, int> pairNext; // (id, idx)

    // data
    Sim sim;

    // callback
    void ActionCallback(Action const &action);

  public:
    // constructors
    ThresholdAnalyzer(Parameter const *pPar);
    void AssignNetwork(Ntk *pNtk_, bool fReuse);

    // interface
    T GetThreshold() const;
    void SetThreshold(T tThreshold_);
    T GetCurrent();
    T GetNext() const;
    std::pair<int, int> GetNextPair() const;
    void ResetNext();
    bool SetBias(std::vector<std::vector<int>> const &vBias);
    std::vector<std::vector<int>> GetContribution();

    // checks
    bool CheckRedundancy(int id, int idx);
    bool CheckFeasibility(int id, int fi, bool c);

    // summary
    void ResetSummary();
    summary<int> GetStatsSummary() const;
    summary<double> GetTimesSummary() const;
  };

  /* {{{ Callback */
  
  template <typename Ntk, typename Sim, typename T, bool fAscending>
  void ThresholdAnalyzer<Ntk, Sim, T, fAscending>::ActionCallback(Action const &action) {
    switch(action.type) {
    case REMOVE_FANIN:
      ResetNext();
      break;
    case REMOVE_UNUSED:
      break;
    case REMOVE_BUFFER:
      break;
    case REMOVE_CONST:
      break;
    case ADD_FANIN:
      assert(0);
      break;
    case TRIVIAL_COLLAPSE:
      // should be handled by optimizer
      break;
    case TRIVIAL_DECOMPOSE:
      assert(0);
      break;
    case SORT_FANINS:
      // should be handled by optimizer (sort per node is fine)
      break;
    case READ:
      assert(0);
      break;
    case SAVE:
      assert(0);
      break;
    case LOAD:
      assert(0);
      break;
    case POP_BACK:
      assert(0);
      break;
    default:
      assert(0);
    }
  }

  /* }}} */

  /* {{{ Constructors */
  
  template <typename Ntk, typename Sim, typename T, bool fAscending>
  ThresholdAnalyzer<Ntk, Sim, T, fAscending>::ThresholdAnalyzer(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nAnalyzerVerbose),
    pairNext({-1, -1}),
    sim(pPar) {
    if constexpr(fAscending) {
      tThreshold = std::numeric_limits<T>::lowest();
      tNext = std::numeric_limits<T>::max();
    } else {
      tThreshold = std::numeric_limits<T>::max();
      tNext = std::numeric_limits<T>::lowest();
    }
  }

  template <typename Ntk, typename Sim, typename T, bool fAscending>
  void ThresholdAnalyzer<Ntk, Sim, T, fAscending>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    pNtk = pNtk_;
    pairNext = {-1, -1};
    if constexpr(fAscending) {
      tNext = std::numeric_limits<T>::max();
    } else {
      tNext = std::numeric_limits<T>::lowest();
    }
    pNtk->AddCallback(std::bind(&ThresholdAnalyzer<Ntk, Sim, T, fAscending>::ActionCallback, this, std::placeholders::_1));
    sim.AssignNetwork(pNtk, fReuse);
    tThreshold = sim.GetDefaultThreshold(); // TODO: GetCurrent()? it doesn't really matter though
  }

  /* }}} */
  
  /* {{{ Interface */

  template <typename Ntk, typename Sim, typename T, bool fAscending>
  T ThresholdAnalyzer<Ntk, Sim, T, fAscending>::GetThreshold() const {
    return tThreshold;
  }
  
  template <typename Ntk, typename Sim, typename T, bool fAscending>
  void ThresholdAnalyzer<Ntk, Sim, T, fAscending>::SetThreshold(T tThreshold_) {
    tThreshold = tThreshold_;
    if constexpr(HasDrop<Sim>) {
      sim.Drop(tThreshold);
    }
  }

  template <typename Ntk, typename Sim, typename T, bool fAscending>
  T ThresholdAnalyzer<Ntk, Sim, T, fAscending>::GetCurrent() {
    return sim.GetCurrent();
  }

  template <typename Ntk, typename Sim, typename T, bool fAscending>
  T ThresholdAnalyzer<Ntk, Sim, T, fAscending>::GetNext() const {
    return tNext;
  }

  template <typename Ntk, typename Sim, typename T, bool fAscending>
  std::pair<int, int> ThresholdAnalyzer<Ntk, Sim, T, fAscending>::GetNextPair() const {
    return pairNext;
  }

  template <typename Ntk, typename Sim, typename T, bool fAscending>
  void ThresholdAnalyzer<Ntk, Sim, T, fAscending>::ResetNext() {
    pairNext = {-1, -1};
    if constexpr(fAscending) {
      tNext = std::numeric_limits<T>::max();
    } else {
      tNext = std::numeric_limits<T>::lowest();
    }
  }

  template <typename Ntk, typename Sim, typename T, bool fAscending>
  bool ThresholdAnalyzer<Ntk, Sim, T, fAscending>::SetBias(std::vector<std::vector<int>> const &vBias) {
    if(sim.SetBias(vBias)) {
      ResetNext();
      return true;
    }
    return false;
  }

  template <typename Ntk, typename Sim, typename T, bool fAscending>
  std::vector<std::vector<int>> ThresholdAnalyzer<Ntk, Sim, T, fAscending>::GetContribution() {
    return sim.GetContribution();
  }
  
  /* }}} */
  
  /* {{{ Checks */
  
  template <typename Ntk, typename Sim, typename T, bool fAscending>
  bool ThresholdAnalyzer<Ntk, Sim, T, fAscending>::CheckRedundancy(int id, int idx) {
    T t = sim.CheckRedundancy(id, idx);
    if constexpr(fAscending) {
      if(t <= tThreshold) {
        return true;
      }
      if(t < tNext) {
        tNext = t;
        pairNext = {id, idx};
      }
    } else {
      if(t >= tThreshold) {
        return true;
      }
      if(t > tNext) {
        tNext = t;
        pairNext = {id, idx};
      }
    }
    return false;
  }
  
  template <typename Ntk, typename Sim, typename T, bool fAscending>
  bool ThresholdAnalyzer<Ntk, Sim, T, fAscending>::CheckFeasibility(int id, int fi, bool c) {
    assert(0);
  }

  /* }}} */

  /* {{{ Summary */
  
  template <typename Ntk, typename Sim, typename T, bool fAscending>
  void ThresholdAnalyzer<Ntk, Sim, T, fAscending>::ResetSummary() {
    sim.ResetSummary();
  }
  
  template <typename Ntk, typename Sim, typename T, bool fAscending>
  summary<int> ThresholdAnalyzer<Ntk, Sim, T, fAscending>::GetStatsSummary() const {
    return sim.GetStatsSummary();
  }

  template <typename Ntk, typename Sim, typename T, bool fAscending>
  summary<double> ThresholdAnalyzer<Ntk, Sim, T, fAscending>::GetTimesSummary() const {
    return sim.GetTimesSummary();
  }

  /* }}} */
  
}
