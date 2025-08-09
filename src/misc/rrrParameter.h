#pragma once

#include <string>

namespace rrr {

  struct Parameter {
    int iSeed = 0;
    int nWords = 10;
    int nTimeout = 0;
    int nSchedulerVerbose = 0;
    int nPartitionerVerbose = 0;
    int nOptimizerVerbose = 0;
    int nAnalyzerVerbose = 0;
    int nSimulatorVerbose = 0;
    int nSatSolverVerbose = 0;
    int nResynVerbose = 0;
    bool fUseBddCspf = false;
    bool fUseBddMspf = false;
    int nConflictLimit = 0;
    int nSortType = -1;
    int nOptimizerFlow = 0;
    int nSchedulerFlow = 0;
    int nPartitionType = 0;
    int nDistance = 0;
    int nJobs = 1;
    int nThreads = 1;
    int nPartitionSize = 0;
    int nPartitionSizeMin = 0;
    int nPartitionInputMax = 0;
    int nResynSize = 30;
    int nResynInputMax = 16;
    int nHops = 10;
    int nJumps = 100;
    bool fDeterministic = true;
    int nParallelPartitions = 1;
    bool fOptOnInsert = false;
    bool fGreedy = true;
    bool fExSim = false;
    bool fNoGlobalJump = false;
    int nRelaxedPatterns = 0;
    bool fUseApprox = false;
    int nReductionMethod = 0;
    bool fSortInitial = false;
    bool fSortPerNode = true;
    std::string strTemporary;
    std::string strPattern;
    std::string strPatternOutput;
    std::string strCond;
    std::string strOutput;
  };
  
}
