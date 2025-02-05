#pragma once

#include <iomanip>
#include <iostream>
#include <chrono>

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
    auto start = std::chrono::system_clock::now();
    Opt opt(pNtk, pPar);
    opt.Run();
    auto end = std::chrono::system_clock::now();
    double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
    if(nVerbose) {
      std::cout << "elapsed: " << std::fixed << std::setprecision(3) << elapsed_seconds << "s" << std::endl;
    }
    //pNtk->Print();
  }

}
