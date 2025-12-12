#pragma once

#include "scheduler/rrrScheduler.h"
#include "scheduler/rrrScheduler3.h"
#include "optimizer/rrrOptimizer.h"
#include "analyzer/rrrTtAnalyzer.h"
#include "analyzer/rrrBddCspfAnalyzer.h"
#include "analyzer/rrrBddMspfAnalyzer.h"
#include "analyzer/rrrBddResimAnalyzer.h"
#include "analyzer/rrrAnalyzer.h"
#include "analyzer/sat/rrrSatSolver.h"
#include "analyzer/sat/rrrSatSolver2.h"
#include "simulator/rrrSimulator.h"
#include "simulator/rrrExhaustiveSimulator.h"
#include "simulator/rrrDcSimulator.h"
#include "partitioner/rrrPartitioner.h"
#include "partitioner/rrrLevelBasePartitioner.h"
#include "extra/rrrPattern.h"

namespace rrr {

  template <typename Ntk, template<typename, typename, typename> typename Sch, template<typename, typename> typename Opt, template<typename> typename Par>
  void PerformInt(Ntk *pNtk, Parameter const *pPar) {
    if(pPar->fUseTtResim) {
      Sch<Ntk, Opt<Ntk, TtAnalyzer<Ntk, ExhaustiveSimulator<Ntk>>>, Par<Ntk>> sch(pNtk, pPar);
      sch.Run();
    } else if(pPar->fUseTt) {
      Sch<Ntk, Opt<Ntk, TtAnalyzer<Ntk, DcSimulator<Ntk>>>, Par<Ntk>> sch(pNtk, pPar);
      sch.Run();
    } else if(pPar->fUseBddResim) {
      Sch<Ntk, Opt<Ntk, BddResimAnalyzer<Ntk>>, Par<Ntk>> sch(pNtk, pPar);
      sch.Run();
    } else if(pPar->fUseBddCspf) {
      Sch<Ntk, Opt<Ntk, BddCspfAnalyzer<Ntk>>, Par<Ntk>> sch(pNtk, pPar);
      sch.Run();
    } else if(pPar->fUseBddMspf) {
      Sch<Ntk, Opt<Ntk, BddMspfAnalyzer<Ntk>>, Par<Ntk>> sch(pNtk, pPar);
      sch.Run();
    } else {
      Sch<Ntk, Opt<Ntk, Analyzer<Ntk, Simulator<Ntk>, SatSolver2<Ntk>>>, Par<Ntk>> sch(pNtk, pPar);
      sch.Run();
    }
  }

  template <typename Ntk>
  void PerformHelo(Ntk *pNtk, Parameter const *pPar) {
    Pattern *pPat = NULL;
    if(!pPar->strPattern.empty()) {
      pPat = new Pattern;
      pPat->Read(pPar->strPattern, pNtk->GetNumPis());
      if(!pPar->strPatternOutput.empty()) {
        pPat->ReadOutput(pPar->strPatternOutput, pNtk->GetNumPos());
      }
      pNtk->RegisterPattern(pPat);
    }
    Ntk *pCond = NULL;
    if(!pPar->strCond.empty()) {
      char buf[100];
      strcpy(buf, pPar->strCond.c_str());
      Gia_Man_t *pGia = Gia_AigerRead(buf, 0, 0, 0);
      pCond = new Ntk;
      pCond->Read(pGia, rrr::GiaReader<Ntk>);
      Gia_ManStop(pGia);
      pNtk->RegisterCond(pCond);
    }
    switch(pPar->nPartitionType) {
    case 0:
      PerformInt<Ntk, Scheduler, Optimizer, Partitioner>(pNtk, pPar);
      break;
    case 1:
      PerformInt<Ntk, Scheduler, Optimizer, LevelBasePartitioner>(pNtk, pPar);
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

  template <typename Ntk>
  void Perform3(Ntk *pNtk, Parameter const *pPar) {
    switch(pPar->nPartitionType) {
    case 0:
      PerformInt<Ntk, Scheduler3, Optimizer, Partitioner>(pNtk, pPar);
      break;
    case 1:
      PerformInt<Ntk, Scheduler3, Optimizer, LevelBasePartitioner>(pNtk, pPar);
      break;
    default:
      assert(0);
    }
  }

}
