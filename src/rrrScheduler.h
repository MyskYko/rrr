#pragma once

#include <iostream>
#include <iomanip>

#include "rrrParameter.h"
#include "rrrTypes.h"

namespace rrr {

  template <typename Ntk, typename Opt>
  class Scheduler {
  private:
    // pointer to network
    Ntk *pNtk;

    // copy of parameter (maybe updated during the run)
    Parameter Par;

  public:
    // constructor
    Scheduler(Ntk *pNtk, Parameter const *pPar);

    // run
    void Run();
  };

  /* {{{ Constructor */
  
  template <typename Ntk, typename Opt>
  Scheduler<Ntk, Opt>::Scheduler(Ntk *pNtk, Parameter const *pPar) :
    pNtk(pNtk),
    Par(*pPar) {
    // TODO: allocations and other preparations should be done here
  }

  /* }}} */

  /* {{{ Run */
  
  // TODO: have multiplie optimizers running on different windows/partitions
  // TODO: run ABC on those windows or even on the entire network, maybe as a separate class
  
  template <typename Ntk, typename Opt>
  void Scheduler<Ntk, Opt>::Run() {
    //pNtk->Print();
    time_point start = GetCurrentTime();
    Opt opt(pNtk, &Par);
    opt.Run(Par.nTimeout);
    time_point end = GetCurrentTime();
    double elapsed_seconds = Duration(start, end);
    if(Par.nSchedulerVerbose) {
      std::cout << "elapsed: " << std::fixed << std::setprecision(3) << elapsed_seconds << "s" << std::endl;
    }
    //pNtk->Print();
  }

  /* }}} */

}
