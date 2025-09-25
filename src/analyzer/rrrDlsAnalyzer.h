#pragma once

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"

namespace rrr {

  template <typename Ntk, typename Sim>
  class DlsAnalyzer {
  private:
    // pointer to network
    Ntk *pNtk;

    // parameters
    bool nVerbose;
    double dThreshold;
    double dMinimum;

    // data
    Sim sim;

  public:
    // constructors
    DlsAnalyzer(Parameter const *pPar);
    void AssignNetwork(Ntk *pNtk_, bool fReuse);

    // interface
    void SetBias(std::vector<std::vector<int>> const &vBias);
    std::vector<std::vector<int>> GetOutputs();

    // loss
    double GetLoss();
    void SetThreshold(double dThreshold_);
    double GetMinimum() const;
    void ResetMinimum();

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
  DlsAnalyzer<Ntk, Sim>::DlsAnalyzer(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nAnalyzerVerbose),
    dThreshold(0),
    dMinimum(std::numeric_limits<double>::max()),
    sim(pPar) {
  }

  template <typename Ntk, typename Sim>
  void DlsAnalyzer<Ntk, Sim>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    pNtk = pNtk_;
    sim.AssignNetwork(pNtk, fReuse);
  }

  /* }}} */

  /* {{{ Interface */

  template <typename Ntk, typename Sim>
  void DlsAnalyzer<Ntk, Sim>::SetBias(std::vector<std::vector<int>> const &vBias) {
    sim.SetBias(vBias);
  }

  template <typename Ntk, typename Sim>
  std::vector<std::vector<int>> DlsAnalyzer<Ntk, Sim>::GetOutputs() {
    return sim.GetOutputs();
  }
  
  /* }}} */
  
  /* {{{ Loss */

  template <typename Ntk, typename Sim>
  double DlsAnalyzer<Ntk, Sim>::GetLoss() {
    return sim.GetLoss();
  }
  
  template <typename Ntk, typename Sim>
  void DlsAnalyzer<Ntk, Sim>::SetThreshold(double dThreshold_) {
    dThreshold = dThreshold_;
  }

  template <typename Ntk, typename Sim>
  double DlsAnalyzer<Ntk, Sim>::GetMinimum() const {
    return dMinimum;
  }
  
  template <typename Ntk, typename Sim>
  void DlsAnalyzer<Ntk, Sim>::ResetMinimum() {
    dMinimum = std::numeric_limits<double>::max();
  }

  /* }}} */
  
  /* {{{ Checks */
  
  template <typename Ntk, typename Sim>
  bool DlsAnalyzer<Ntk, Sim>::CheckRedundancy(int id, int idx) {
    double loss = sim.CheckRedundancy(id, idx);
    if(loss <= dThreshold) {
      return true;
    }
    if(loss < dMinimum) {
      dMinimum = loss;
    }
    return false;
  }
  
  template <typename Ntk, typename Sim>
  bool DlsAnalyzer<Ntk, Sim>::CheckFeasibility(int id, int fi, bool c) {
    assert(0);
  }

  /* }}} */

  /* {{{ Summary */
  
  template <typename Ntk, typename Sim>
  void DlsAnalyzer<Ntk, Sim>::ResetSummary() {
    sim.ResetSummary();
  }
  
  template <typename Ntk, typename Sim>
  summary<int> DlsAnalyzer<Ntk, Sim>::GetStatsSummary() const {
    return sim.GetStatsSummary();
  }

  template <typename Ntk, typename Sim>
  summary<double> DlsAnalyzer<Ntk, Sim>::GetTimesSummary() const {
    return sim.GetTimesSummary();
  }

  /* }}} */
  
}
