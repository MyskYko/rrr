#pragma once

namespace rrr {
  
  struct Parameter {
    int nWords = 1;
    int iSeed = 0;
    int nSchedulerVerbose = 0;
    int nOptimizerVerbose = 0;
    int nAnalyzerVerbose = 0;
    bool fUseBddCspf = false;
    bool fUseBddMspf = false;
  };
  
}
