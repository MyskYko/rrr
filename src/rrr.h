#pragma once

#include "scheduler/rrrScheduler.h"
#include "scheduler/rrrScheduler2.h"
#include "scheduler/rrrScheduler3.h"
#include "optimizer/rrrOptimizer.h"
#include "optimizer/rrrOptimizer2.h"
#include "optimizer/rrrOptimizer3.h"
#include "analyzer/rrrApproxAnalyzer.h"
#include "analyzer/rrrBddAnalyzer.h"
#include "analyzer/rrrBddMspfAnalyzer.h"
#include "analyzer/rrrAnalyzer.h"
#include "analyzer/sat/rrrSatSolver.h"
#include "simulator/rrrSimulator.h"
#include "simulator/rrrSimulator2.h"
#include "simulator/rrrSimulator3.h"
#include "partitioner/rrrPartitioner.h"
#include "partitioner/rrrLevelBasePartitioner.h"
#include "extra/rrrPattern.h"

namespace rrr {

  template <typename Ntk, template<typename, typename, typename> typename Sch, template<typename, typename> typename Opt, template<typename> typename Par>
  void PerformInt(Ntk *pNtk, Parameter const *pPar) {
    assert(!pPar->fUseBddCspf || !pPar->fUseBddMspf);
    if(pPar->fUseBddCspf) {
      Sch<Ntk, Opt<Ntk, BddAnalyzer<Ntk>>, Par<Ntk>> sch(pNtk, pPar);
      sch.Run();
    } else if(pPar->fUseBddMspf) {
      Sch<Ntk, Opt<Ntk, BddMspfAnalyzer<Ntk>>, Par<Ntk>> sch(pNtk, pPar);
      sch.Run();
    } else {
      Sch<Ntk, Opt<Ntk, Analyzer<Ntk, Simulator<Ntk>, SatSolver<Ntk>>>, Par<Ntk>> sch(pNtk, pPar);
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
  void PerformAls(Ntk *pNtk, Parameter const *pPar) {
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
    assert(!pPar->fUseBddCspf && !pPar->fUseBddMspf);
    switch(pPar->nPartitionType) {
    case 0: {
      Scheduler<Ntk, Optimizer3<Ntk, ApproxAnalyzer<Ntk, Simulator3<Ntk>>>, Partitioner<Ntk>> sch(pNtk, pPar);
      sch.Run();
      break;
    }
    case 1: {
      Scheduler<Ntk, Optimizer3<Ntk, ApproxAnalyzer<Ntk, Simulator3<Ntk>>>, LevelBasePartitioner<Ntk>> sch(pNtk, pPar);
      sch.Run();
      break;
    }
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
  void PerformSsra(Ntk *pNtk, Parameter const *pPar) {
    switch(pPar->nPartitionType) {
    case 0:
      PerformInt<Ntk, Scheduler2, Optimizer2, Partitioner>(pNtk, pPar);
      break;
    case 1:
      PerformInt<Ntk, Scheduler2, Optimizer2, LevelBasePartitioner>(pNtk, pPar);
      break;
    default:
      assert(0);
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
