#pragma once

#include "rrrAndNetwork.h"
#include "rrrScheduler.h"
#include "rrrOptimizer.h"
#include "rrrBddAnalyzer.h"
#include "rrrBddMspfAnalyzer.h"
#include "rrrAnalyzer.h"
#include "rrrApproxAnalyzer.h"
#include "rrrSatSolver.h"
#include "rrrSimulator.h"
#include "rrrSimulator2.h"
#include "rrrPartitioner.h"
#include "rrrLevelBasePartitioner.h"
#include "rrrPattern.h"

namespace rrr {

  template <typename Ntk>
  void Perform(Ntk *pNtk, Parameter const *pPar) {
    Pattern *pPat = NULL;
    if(!pPar->strPattern.empty()) {
      pPat = new Pattern;
      pPat->Read(pPar->strPattern, pNtk->GetNumPis());
      pNtk->RegisterPattern(pPat);
    }
    Ntk *pCond = NULL;
    if(!pPar->strCond.empty()) {
      char buf[100];
      strcpy(buf, pPar->strCond.c_str());
      Gia_Man_t *pGia = Gia_AigerRead(buf, 0, 0, 0);
      pCond = new AndNetwork;
      pCond->Read(pGia, rrr::GiaReader<rrr::AndNetwork>);
      Gia_ManStop(pGia);
      pNtk->RegisterCond(pCond);
    }
    assert(!pPar->fUseBddCspf || !pPar->fUseBddMspf);
    switch(pPar->nPartitionType) {
    case 0:
      if(pPar->fUseBddCspf) {
        Scheduler<Ntk, Optimizer<Ntk, BddAnalyzer<Ntk>>, Partitioner<Ntk>> sch(pNtk, pPar);
        sch.Run();
      } else if(pPar->fUseBddMspf) {
        Scheduler<Ntk, Optimizer<Ntk, BddMspfAnalyzer<Ntk>>, Partitioner<Ntk>> sch(pNtk, pPar);
        sch.Run();
      } else {
        Scheduler<Ntk, Optimizer<Ntk, ApproxAnalyzer<Ntk, Simulator2<Ntk>>>, Partitioner<Ntk>> sch(pNtk, pPar);
        sch.Run();
      }
      break;
    case 1:
      if(pPar->fUseBddCspf) {
        Scheduler<Ntk, Optimizer<Ntk, BddAnalyzer<Ntk>>, LevelBasePartitioner<Ntk>> sch(pNtk, pPar);
        sch.Run();
      } else if(pPar->fUseBddMspf) {
        Scheduler<Ntk, Optimizer<Ntk, BddMspfAnalyzer<Ntk>>, LevelBasePartitioner<Ntk>> sch(pNtk, pPar);
        sch.Run();
      } else {
        Scheduler<Ntk, Optimizer<Ntk, Analyzer<Ntk, Simulator<Ntk>, SatSolver<Ntk>>>, LevelBasePartitioner<Ntk>> sch(pNtk, pPar);
        sch.Run();
      }
      break;
    default:
      assert(0);
    }
    if(pPat) {
      delete pPat;
    }
    if(pCond) {
      delete pCond;
    }
  }
  
}
