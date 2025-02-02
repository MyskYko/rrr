#pragma once

#include <chrono>

#include "rrrAndNetwork.h"
#include "rrrParameter.h"
#include "rrrOptimizer.h"
#include "rrrAnalyzer.h"
#include "rrrBddAnalyzer.h"
#include "rrrBddMspfAnalyzer.h"
#include "rrrSimulator.h"
#include "rrrSolver.h"


namespace rrr {

  template <typename Ntk, typename Ana>
  void Perform(Ntk *pNtk, Parameter *pPar, int fVerbose) {

    auto start = std::chrono::system_clock::now();
    
    Optimizer<Ntk, Ana> opt(pNtk, pPar);
    if(fVerbose) {
      pNtk->Print();
    }
    opt.RemoveRedundancy();
    //opt.RemoveRedundancyRandom();
    //opt.Reduce();
    //opt.ReduceRandom();
    //if(pPar->fVerbose) {
    //pNtk->Print();
    //}
    //opt.SingleResubRandom(1000);
    //opt.SingleResubRandom(10, 5);
    opt.SingleResub();
    if(fVerbose) {
    pNtk->Print();
    }
    // opt.RemoveRedundancy();
    // if(pPar->fVerbose) {
    //   pNtk->Print();
    // }

    auto end = std::chrono::system_clock::now();
    double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
    std::cout << "elapsed: " << std::fixed << std::setprecision(3) << elapsed_seconds << "s" << std::endl;

  }
  
}
