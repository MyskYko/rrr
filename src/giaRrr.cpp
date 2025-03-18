#include "aig/gia/gia.h"

#include "rrr.h"
#include "rrrAbc.h"

extern "C"
Gia_Man_t *Gia_ManRrr(Gia_Man_t *pGia, int iSeed, int nWords, int nTimeout, int nSchedulerVerbose, int nPartitionerVerbose, int nOptimizerVerbose, int nAnalyzerVerbose, int nSimulatorVerbose, int nSatSolverVerbose, int fUseBddCspf, int fUseBddMspf, int nConflictLimit, int nSortType, int nOptimizerFlow, int nSchedulerFlow, int nDistance, int nTasks, int nThreads, int nPartitionSize, int fDeterministic) {
  rrr::AndNetwork ntk;
  ntk.Read(pGia, rrr::GiaReader<rrr::AndNetwork>);
  rrr::Parameter Par;
  Par.iSeed = iSeed;
  Par.nWords = nWords;
  Par.nTimeout = nTimeout;
  Par.nSchedulerVerbose = nSchedulerVerbose;
  Par.nPartitionerVerbose = nPartitionerVerbose;
  Par.nOptimizerVerbose = nOptimizerVerbose;
  Par.nAnalyzerVerbose = nAnalyzerVerbose;
  Par.nSimulatorVerbose = nSimulatorVerbose;
  Par.nSatSolverVerbose = nSatSolverVerbose;
  Par.fUseBddCspf = fUseBddCspf;
  Par.fUseBddMspf = fUseBddMspf;
  Par.nConflictLimit = nConflictLimit;
  Par.nSortType = nSortType;
  Par.nOptimizerFlow = nOptimizerFlow;
  Par.nSchedulerFlow = nSchedulerFlow;
  Par.nDistance = nDistance;
  Par.nTasks = nTasks;
  Par.nThreads = nThreads;
  Par.nPartitionSize = nPartitionSize;
  Par.fDeterministic = fDeterministic;
  rrr::Perform(&ntk, &Par);
  Gia_Man_t *pNew = rrr::CreateGia(&ntk);
  return pNew;
}
