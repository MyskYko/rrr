#pragma once

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"

namespace rrr {

  template <typename Ntk, typename Sim>
  class TtAnalyzer {
  private:
    // pointer to network
    Ntk *pNtk;

    // parameters
    bool nVerbose;

    // data
    //ExhaustiveSimulator<Ntk> sim;
    Sim sim;

  public:
    // constructors
    TtAnalyzer(Parameter const *pPar);
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
  TtAnalyzer<Ntk, Sim>::TtAnalyzer(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nAnalyzerVerbose),
    sim(pPar) {
  }

  template <typename Ntk, typename Sim>
  void TtAnalyzer<Ntk, Sim>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    pNtk = pNtk_;
    sim.AssignNetwork(pNtk, fReuse);
  }

  /* }}} */

  /* {{{ Checks */
  
  template <typename Ntk, typename Sim>
  bool TtAnalyzer<Ntk, Sim>::CheckRedundancy(int id, int idx) {
    return sim.CheckRedundancy(id, idx);
  }
  
  template <typename Ntk, typename Sim>
  bool TtAnalyzer<Ntk, Sim>::CheckFeasibility(int id, int fi, bool c) {
    return sim.CheckFeasibility(id, fi, c);
  }

  /* }}} */

  /* {{{ Summary */
  
  template <typename Ntk, typename Sim>
  void TtAnalyzer<Ntk, Sim>::ResetSummary() {
    sim.ResetSummary();
  }
  
  template <typename Ntk, typename Sim>
  summary<int> TtAnalyzer<Ntk, Sim>::GetStatsSummary() const {
    return sim.GetStatsSummary();
  }

  template <typename Ntk, typename Sim>
  summary<double> TtAnalyzer<Ntk, Sim>::GetTimesSummary() const {
    return sim.GetTimesSummary();
  }

  /* }}} */
  
}
