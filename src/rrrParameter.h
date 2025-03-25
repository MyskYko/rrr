#pragma once

namespace rrr {

  struct Parameter {
    int iSeed = 0;
    int nWords = 10;
    int nTimeout = 0;
    int nSchedulerVerbose = 1;
    int nPartitionerVerbose = 0;
    int nOptimizerVerbose = 0;
    int nAnalyzerVerbose = 0;
    int nSimulatorVerbose = 0;
    int nSatSolverVerbose = 0;
    bool fUseBddCspf = false;
    bool fUseBddMspf = false;
    int nConflictLimit = 0;
    int nSortType = 12;
    int nOptimizerFlow = 0;
    int nSchedulerFlow = 0;
    int nDistance = 0;
    int nJobs = 1;
    int nThreads = 1;
    int nPartitionSize = 0;
    bool fDeterministic = true;
    int nParallelPartitions = 1;
    bool fOptOnInsert = false;
    bool fGreedy = true;
  };
  
}
