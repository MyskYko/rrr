#pragma once

#include <cassert>

#include "rrrParameter.h"
#include "rrrTypes.h"
#include "rrrAndNetwork.h"
#include "rrrScheduler.h"
#include "rrrOptimizer.h"
#include "rrrAnalyzer.h"
#include "rrrBddAnalyzer.h"
#include "rrrBddMspfAnalyzer.h"
#include "rrrSolver.h"
#include "rrrSimulator.h"

namespace rrr {

  template <typename Ntk>
  void Perform(Ntk *pNtk, Parameter *pPar) {
    // TODO: make pPar const
    assert(!pPar->fUseBddCspf || !pPar->fUseBddMspf);
    if(pPar->fUseBddCspf) {
      Scheduler<rrr::Optimizer<Ntk, rrr::BddAnalyzer<Ntk>>> sch;
      sch.Run(pNtk, pPar);
    } else if(pPar->fUseBddMspf) {
      Scheduler<rrr::Optimizer<Ntk, rrr::BddMspfAnalyzer<Ntk>>> sch;
      sch.Run(pNtk, pPar);
    } else {
      Scheduler<rrr::Optimizer<Ntk, rrr::Analyzer<Ntk, rrr::Simulator<Ntk>, rrr::Solver<Ntk>>>> sch;
      sch.Run(pNtk, pPar);
    }
  }
  
}
