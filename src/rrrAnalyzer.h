#pragma once

#include <iostream>
#include <vector>

#include "rrrParameter.h"


namespace rrr {

  template <typename Ntk, typename Sim, typename Sol>
  class Analyzer {
  private:
    bool nVerbose;
    Ntk *pNtk;
    Sim sim;
    Sol sol;

  public:
    Analyzer(Ntk *pNtk, Parameter *pPar);
    void UpdateNetwork(Ntk *pNtk_);

    bool CheckRedundancy(int id, int idx);
    bool CheckFeasibility(int id, int fi, bool c);
  };


  template <typename Ntk, typename Sim, typename Sol>
  Analyzer<Ntk, Sim, Sol>::Analyzer(Ntk *pNtk, Parameter *pPar) :
    nVerbose(pPar->nAnalyzerVerbose),
    pNtk(pNtk),
    sim(pNtk, pPar),
    sol(pNtk) {
  }
  
  template <typename Ntk, typename Sim, typename Sol>
  void Analyzer<Ntk, Sim, Sol>::UpdateNetwork(Ntk *pNtk_) {
    pNtk = pNtk_;
    sim->UpdateNetwork(pNtk);
    assert(0);
  }

  template <typename Ntk, typename Sim, typename Sol>
  bool Analyzer<Ntk, Sim, Sol>::CheckRedundancy(int id, int idx) {
    if(sim.CheckRedundancy(id, idx)) {
      if(nVerbose) {
        std::cout << "node " << id << " fanin " << idx <<  " (" << (pNtk->GetCompl(id, idx)? "!": "") << pNtk->GetFanin(id, idx) << ") seems redundant" << std::endl;
      }
      if(sol.CheckRedundancy(id, idx)) {
        if(nVerbose) {
          std::cout << "node " << id << " fanin " << idx <<  " (" << (pNtk->GetCompl(id, idx)? "!": "") << pNtk->GetFanin(id, idx) << ") is redundant" << std::endl;
        }
        return true;
      }
      if(nVerbose) {
        std::cout << "node " << id << " fanin " << idx <<  " (" << (pNtk->GetCompl(id, idx)? "!": "") << pNtk->GetFanin(id, idx) << ") is NOT redundant" << std::endl;
      }
      // add counter example
      auto vCex = sol.GetCex();
      sim.AddCex(vCex);
    }
    return false;
  }
  
  template <typename Ntk, typename Sim, typename Sol>
  bool Analyzer<Ntk, Sim, Sol>::CheckFeasibility(int id, int fi, bool c) {
    if(sim.CheckFeasibility(id, fi, c)) {
      if(nVerbose) {
        std::cout << "node " << id << " fanin (" << (c? "!": "") << fi << ") seems feasible" << std::endl;
      }
      if(sol.CheckFeasibility(id, fi, c)) {
        if(nVerbose) {
          std::cout << "node " << id << " fanin (" << (c? "!": "") << fi << ") is feasible" << std::endl;
        }
        return true;
      }
      if(nVerbose) {
        std::cout << "node " << id << " fanin (" << (c? "!": "") << fi << ") is NOT feasible" << std::endl;
      }
      // add counter example
      auto vCex = sol.GetCex();
      sim.AddCex(vCex);
    }
    return false;
  }
  
}
