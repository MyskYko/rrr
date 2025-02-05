#pragma once

#include <iostream>
#include <iomanip>

#include "rrrTypes.h"
#include "rrrParameter.h"

namespace rrr {

  template <typename Opt>
  class Scheduler {
  private:
    int nVerbose;

  public:
    Scheduler();
    
    template <typename Ntk>
    void Run(Ntk *pNtk, Parameter *pPar);
  };

  template <typename Opt>
  Scheduler<Opt>::Scheduler() :
    nVerbose(0) {
  }

  // TODO: have multiplie optimizers running on different windows/partitions
  // TODO: run ABC on those windows or even on the entire network, maybe as a separate class
  
  template <typename Opt>
  template <typename Ntk>
  void Scheduler<Opt>::Run(Ntk *pNtk, Parameter *pPar) {
    nVerbose = pPar->nSchedulerVerbose;
    //pNtk->Print();
    time_point start = GetCurrentTime();
    Opt opt(pNtk, pPar);
    opt.Run(pPar->nTimeout);
    time_point end = GetCurrentTime();
    double elapsed_seconds = Duration(start, end);
    if(nVerbose) {
      std::cout << "elapsed: " << std::fixed << std::setprecision(3) << elapsed_seconds << "s" << std::endl;
    }
    //pNtk->Print();
  }

}
