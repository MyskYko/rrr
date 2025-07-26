#include "aig/gia/gia.h"

#include "network/rrrAndNetwork.h"
#include "interface/rrrAbc.h"
#include "rrr.h"

//extern "C"
Gia_Man_t *Gia_ManRrr(Gia_Man_t *pGia, int iSeed, int nWords, int nTimeout, int nSchedulerVerbose, int nPartitionerVerbose, int nOptimizerVerbose, int nAnalyzerVerbose, int nSimulatorVerbose, int nSatSolverVerbose, int nResynVerbose, int fUseBddCspf, int fUseBddMspf, int nConflictLimit, int nSortType, int nOptimizerFlow, int nSchedulerFlow, int nPartitionType, int nDistance, int nJobs, int nThreads, int nPartitionSize, int nPartitionSizeMin, int nPartitionInputMax, int nResynSize, int nResynInputMax, int fDeterministic, int nParallelPartitions, int nHops, int nJumps, int fOptOnInsert, int fGreedy, int fExSim, int fNoGlobalJump, int nRelaxedPatterns, char *pTemporary, char *pPattern, char *pCond, char *pOutput) {
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
  Par.nResynVerbose = nResynVerbose;
  Par.fUseBddCspf = fUseBddCspf;
  Par.fUseBddMspf = fUseBddMspf;
  Par.nConflictLimit = nConflictLimit;
  Par.nSortType = nSortType;
  Par.nOptimizerFlow = nOptimizerFlow;
  Par.nSchedulerFlow = nSchedulerFlow;
  Par.nPartitionType = nPartitionType;
  Par.nDistance = nDistance;
  Par.nJobs = nJobs;
  Par.nThreads = nThreads;
  Par.nPartitionSize = nPartitionSize;
  Par.nPartitionSizeMin = nPartitionSizeMin;
  Par.nPartitionInputMax = nPartitionInputMax;
  Par.nResynSize = nResynSize;
  Par.nResynInputMax = nResynInputMax;
  Par.nHops = nHops;
  Par.nJumps = nJumps;
  Par.fDeterministic = fDeterministic;
  Par.nParallelPartitions = nParallelPartitions;
  Par.fOptOnInsert = fOptOnInsert;
  Par.fGreedy = fGreedy;
  Par.fExSim = fExSim;
  Par.fNoGlobalJump = fNoGlobalJump;
  Par.nRelaxedPatterns = nRelaxedPatterns;
  if(pTemporary) {
    Par.strTemporary = std::string(pTemporary);
  }
  if(pPattern) {
    Par.strPattern = std::string(pPattern);
  }
  if(pCond) {
    Par.strCond = std::string(pCond);
  }
  if(pOutput) {
    Par.strOutput = pOutput;
  }
  rrr::Perform(&ntk, &Par);
  Gia_Man_t *pNew = rrr::CreateGia(&ntk, false);
  return pNew;
}
