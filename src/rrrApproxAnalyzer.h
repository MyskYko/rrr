#pragma once

#include "rrrParameter.h"
#include "rrrUtils.h"

namespace rrr {

  template <typename Ntk, typename Sim>
  class ApproxAnalyzer {
  private:
    // pointer to network
    Ntk *pNtk;

    // parameters
    bool nVerbose;

    // data
    Sim sim;

  public:
    // constructors
    ApproxAnalyzer(Parameter const *pPar);
    void AssignNetwork(Ntk *pNtk_, bool fReuse);

    // checks
    bool CheckRedundancy(int id, int idx);
    bool CheckFeasibility(int id, int fi, bool c);

    // summary
    void ResetSummary();
    summary<int> GetStatsSummary() const;
    summary<double> GetTimesSummary() const;
  };

  /* {{{ Constructors */
  
  template <typename Ntk, typename Sim>
  ApproxAnalyzer<Ntk, Sim>::ApproxAnalyzer(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nAnalyzerVerbose),
    sim(pPar) {
  }

  template <typename Ntk, typename Sim>
  void ApproxAnalyzer<Ntk, Sim>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    pNtk = pNtk_;
    sim.AssignNetwork(pNtk, fReuse);
  }

  /* }}} */

  /* {{{ Checks */
  
  template <typename Ntk, typename Sim>
  bool ApproxAnalyzer<Ntk, Sim>::CheckRedundancy(int id, int idx) {
    if(sim.CheckRedundancy(id, idx)) {
      if(nVerbose) {
        std::cout << "node " << id << " fanin " << (pNtk->GetCompl(id, idx)? "!": "") << pNtk->GetFanin(id, idx) << " index " << idx << " seems redundant" << std::endl;
      }
      return true;
    }
    // if(nVerbose) {
    //   std::cout << "node " << id << " fanin " << (pNtk->GetCompl(id, idx)? "!": "") << pNtk->GetFanin(id, idx) << " index " << idx << " is not redundant" << std::endl;
    // }
    return false;
  }
  
  template <typename Ntk, typename Sim>
  bool ApproxAnalyzer<Ntk, Sim>::CheckFeasibility(int id, int fi, bool c) {
    if(sim.CheckFeasibility(id, fi, c)) {
      if(nVerbose) {
        std::cout << "node " << id << " fanin " << (c? "!": "") << fi << " seems feasible" << std::endl;
      }
      return true;
    }
    // if(nVerbose) {
    //   std::cout << "node " << id << " fanin " << (c? "!": "") << fi << " is not feasible" << std::endl;
    // }
    return false;
  }

  /* }}} */

  /* {{{ Summary */
  
  template <typename Ntk, typename Sim>
  void ApproxAnalyzer<Ntk, Sim>::ResetSummary() {
    sim.ResetSummary();
  }
  
  template <typename Ntk, typename Sim>
  summary<int> ApproxAnalyzer<Ntk, Sim>::GetStatsSummary() const {
    return sim.GetStatsSummary();
  }

  template <typename Ntk, typename Sim>
  summary<double> ApproxAnalyzer<Ntk, Sim>::GetTimesSummary() const {
    return sim.GetTimesSummary();
  }

  /* }}} */
  
}
