#include <base/abc/abc.h>
#include <base/main/main.h>
#include <base/main/mainInt.h>
#include <aig/gia/giaAig.h>
// #include <misc/st/st.h>
// #include <map/mio/mio.h>
// #include <map/amap/amap.h>
#include <proof/cec/cec.h>

#define PARAMS iSeed, nWords, nTimeout, nSchedulerVerbose, nPartitionerVerbose, nOptimizerVerbose, nAnalyzerVerbose, nSimulatorVerbose, nSatSolverVerbose, nResynVerbose, fUseBddCspf, fUseBddMspf, nConflictLimit, nSortType, nOptimizerFlow, nSchedulerFlow, nPartitionType, nDistance, nJobs, nThreads, nPartitionSize, nPartitionSizeMin, nPartitionInputMax, nResynSize, nResynInputMax, fDeterministic, nParallelPartitions, nHops, nJumps, fOptOnInsert, fGreedy, fExSim, fNoGlobalJump, nRelaxedPatterns, pTemporary, pPattern, pCond, pOutput
#define PARAMS_DEF int iSeed = 0, nWords = 10, nTimeout = 0, nSchedulerVerbose = 0, nPartitionerVerbose = 0, nOptimizerVerbose = 0, nAnalyzerVerbose = 0, nSimulatorVerbose = 0, nSatSolverVerbose = 0, nResynVerbose = 0, fUseBddCspf = 0, fUseBddMspf = 0, nConflictLimit = 0, nSortType = -1, nOptimizerFlow = 0, nSchedulerFlow = 0, nPartitionType = 0, nDistance = 0, nJobs = 1, nThreads = 1, nPartitionSize = 0, nPartitionSizeMin = 0, nPartitionInputMax = 0, nResynSize = 30, nResynInputMax = 16, fDeterministic = 1, nParallelPartitions = 1, nHops = 10, nJumps = 100, fOptOnInsert = 0, fGreedy = 1, fExSim = 0, fNoGlobalJump = 0, nRelaxedPatterns = 0; char *pTemporary = NULL, *pPattern = NULL, *pCond = NULL, *pOutput = NULL
#define PARAMS_DECL int iSeed, int nWords, int nTimeout, int nSchedulerVerbose, int nPartitionerVerbose, int nOptimizerVerbose, int nAnalyzerVerbose, int nSimulatorVerbose, int nSatSolverVerbose, int nResynVerbose, int fUseBddCspf, int fUseBddMspf, int nConflictLimit, int nSortType, int nOptimizerFlow, int nSchedulerFlow, int nPartitionType, int nDistance, int nJobs, int nThreads, int nPartitionSize, int nPartitionSizeMin, int nPartitionInputMax, int nResynSize, int nResynInputMax, int fDeterministic, int nParallelPartitions, int nHops, int nJumps, int fOptOnInsert, int fGreedy, int fExSim, int fNoGlobalJump, int nRelaxedPatterns, char *pTemporary, char *pPattern, char *pCond, char *pOutput

using namespace aabbcc;

extern Gia_Man_t *Gia_ManRrr(Gia_Man_t *pGia, PARAMS_DECL);
Gia_Man_t *MinimizeTest(Gia_Man_t *pGia, PARAMS_DECL);


int main(int argc, char **argv) {
  Abc_Start();

  Gia_Man_t *pNew;
  int c;
  PARAMS_DEF;
  int fCore = 0, fVerify = 0;
  Extra_UtilGetoptReset();
  while ( ( c = Extra_UtilGetopt( argc, argv, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefgijklmvoh" ) ) != EOF )
  {
      switch ( c )
      {
      case 'A':
          nAnalyzerVerbose = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'B':
          nParallelPartitions = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'C':
          nConflictLimit = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'D':
          nDistance = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'E':
          nResynSize = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'F':
          nResynInputMax = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'G':
          nSortType = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'H':
          nHops = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'I':
          nPartitionInputMax = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'J':
          nThreads = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'K':
          nPartitionSize = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'L':
          nPartitionSizeMin = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'M':
          nJumps = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'N':
          nJobs = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'O':
          nOptimizerVerbose = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'P':
          nPartitionerVerbose = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'Q':
          nSimulatorVerbose = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'R':
          iSeed = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'S':
          nSatSolverVerbose = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'T':
          nTimeout = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'U':
          nResynVerbose = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'V':
          nSchedulerVerbose = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'W':
          nWords = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'X':
          nOptimizerFlow = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'Y':
          nSchedulerFlow = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'Z':
          nPartitionType = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'a':
          fUseBddCspf ^= 1;
          break;
      case 'b':
          fUseBddMspf ^= 1;
          break;
      case 'c':
          fCore ^= 1;
          break;
      case 'd':
          fDeterministic ^= 1;
          break;
      case 'e':
          fOptOnInsert ^= 1;
          break;
      case 'f':
          fExSim ^= 1;
          break;
      case 'g':
          fGreedy ^= 1;
          break;
      case 'm':
	  fNoGlobalJump ^= 1;
	  break;
      case 'i':
          pCond = argv[globalUtilOptind];
          globalUtilOptind++;
          break;
      case 'j':
          pTemporary = argv[globalUtilOptind];
          globalUtilOptind++;
          break;
      case 'k':
          pPattern = argv[globalUtilOptind];
          globalUtilOptind++;
          break;
      case 'l':
          nRelaxedPatterns = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'o':
          pOutput = argv[globalUtilOptind];
          globalUtilOptind++;
          break;
      case 'v':
          fVerify ^= 1;
          break;
      case 'h':
          goto usage;
      default:
          goto usage;
      }
  }
  
  if(argc != globalUtilOptind + 1) {
    goto usage;
  }

  {
  
  char * fname = argv[globalUtilOptind];
  Gia_Man_t *pGia = Gia_AigerRead(fname, 0, 0, 0);

  if(nSchedulerVerbose) {
    Abc_Print( 2, "Using the following parameters :\n" );
    Abc_Print( 2, "\t-X %3d : method ", nOptimizerFlow );
    switch(nOptimizerFlow) {
    case 0:
      Abc_Print( 2, "(0 = single-add resub)" );
      break;
    case 1:
      Abc_Print( 2, "(1 = multi-add resub)" );
      break;
    case 2:
      Abc_Print( 2, "(2 = repeat single-add and multi-add resubs)" );
      break;
    case 3:
      Abc_Print( 2, "(3 = random one meaningful resub)" );
      break;
    }
    Abc_Print( 2, "\n" );
    Abc_Print( 2, "\t-Y %3d : flow ", nSchedulerFlow );
    switch(nSchedulerFlow) {
    case 0:
      Abc_Print( 2, "(0 = apply method once)" );
      break;
    case 1:
      Abc_Print( 2, "(1 = iterate like transtoch)" );
      break;
    case 2:
      Abc_Print( 2, "(2 = iterate like deepsyn)" );
      break;
    }
    Abc_Print( 2, "\n" );
    Abc_Print( 2, "\t-N %3d : number of jobs to create by restarting or partitioning\n", nJobs );
    Abc_Print( 2, "\t-J %3d : number of threads\n", nThreads );
    Abc_Print( 2, "\t-K %3d : maximum partition size (0 = no partitioning)\n", nPartitionSize );
    Abc_Print( 2, "\t-L %3d : minimum partition size\n", nPartitionSizeMin );
    Abc_Print( 2, "\t-B %3d : maximum number of partitions to optimize in parallel\n", nParallelPartitions );
    Abc_Print( 2, "\t-R %3d : random number generator seed\n", iSeed );
    Abc_Print( 2, "\t-T %3d : timeout in seconds (0 = no timeout)\n", nTimeout );
    Abc_Print( 2, "\t-C %3d : conflict limit (0 = no limit)\n", nConflictLimit );
    Abc_Print( 2, "Use command line switch \"-h\" to see more options\n\n" );
  }

  pNew = Gia_ManRrr(pGia, PARAMS);

  if(fVerify) {
    if(Cec_ManVerifyTwo(pGia, pNew, 0)) {
      printf("equivalent\n");
    } else {
      printf("NOT EQUIVALENT\n");
      if(fCore) {
        Gia_Man_t *pCore = MinimizeTest(pGia, PARAMS);
        Gia_AigerWrite(pCore, "core.aig", 0, 0, 0);
        printf("dumped a core of %d nodes\n", Gia_ManAndNum(pCore));
        Gia_ManStop(pCore);
      }
    }
  }

  if(pOutput != NULL) {
    Gia_AigerWrite(pNew, pOutput, 0, 0, 0);
  }
  
  Gia_ManStop(pGia);
  Gia_ManStop(pNew);
  
  Abc_Stop();
  return 0;
  }

usage:
      Abc_Print( -2, "usage: rrr [-XYZNJKLIEFBDRWTCGVPOAQSUHMl num] [-ijko str] [-abdegmh]\n" );
      Abc_Print( -2, "\t        perform optimization\n" );
      Abc_Print( -2, "\t-X num : method [default = %d]\n", nOptimizerFlow );
      Abc_Print( -2, "\t                0: single-add resub\n" );
      Abc_Print( -2, "\t                1: multi-add resub\n" );
      Abc_Print( -2, "\t                2: repeat 0 and 1\n" );
      Abc_Print( -2, "\t                3: random one meaningful resub\n" );
      Abc_Print( -2, "\t-Y num : flow [default = %d]\n", nSchedulerFlow );
      Abc_Print( -2, "\t                0: apply method once\n" );
      Abc_Print( -2, "\t                1: iterate like transtoch\n" );
      Abc_Print( -2, "\t                2: iterate like deepsyn\n" );
      Abc_Print( -2, "\t-Z num : partition [default = %d]\n", nPartitionType );
      Abc_Print( -2, "\t                0: distance base\n" );
      Abc_Print( -2, "\t                1: level base\n" );
      Abc_Print( -2, "\t-N num : number of jobs to create by restarting or partitioning [default = %d]\n", nJobs );
      Abc_Print( -2, "\t-J num : number of threads [default = %d]\n", nThreads );
      Abc_Print( -2, "\t-K num : maximum partition size (0 = no partitioning) [default = %d]\n", nPartitionSize );
      Abc_Print( -2, "\t-L num : minimum partition size [default = %d]\n", nPartitionSizeMin );
      Abc_Print( -2, "\t-I num : maximum partition input size [default = %d]\n", nPartitionInputMax );
      Abc_Print( -2, "\t-E num : maximum partition size for resynthesis [default = %d]\n", nResynSize );
      Abc_Print( -2, "\t-F num : maximum partition input size for resynthesis [default = %d]\n", nResynInputMax );
      Abc_Print( -2, "\t-B num : maximum number of partitions to optimize in parallel [default = %d]\n", nParallelPartitions );
      Abc_Print( -2, "\t-D num : maximum distance between node and new fanin (0 = no limit) [default = %d]\n", nDistance );
      Abc_Print( -2, "\t-R num : random number generator seed [default = %d]\n", iSeed );
      Abc_Print( -2, "\t-W num : number of simulation words [default = %d]\n", nWords );
      Abc_Print( -2, "\t-T num : timeout in seconds (0 = no timeout) [default = %d]\n", nTimeout );
      Abc_Print( -2, "\t-C num : conflict limit (0 = no limit) [default = %d]\n", nConflictLimit );
      Abc_Print( -2, "\t-G num : fanin cost function (-1 = random) [default = %d]\n", nSortType );
      Abc_Print( -2, "\t-V num : scheduler verbosity level [default = %d]\n", nSchedulerVerbose );
      Abc_Print( -2, "\t-P num : partitioner verbosity level [default = %d]\n", nPartitionerVerbose );
      Abc_Print( -2, "\t-O num : optimizer verbosity level [default = %d]\n", nOptimizerVerbose );
      Abc_Print( -2, "\t-A num : analyzer verbosity level [default = %d]\n", nAnalyzerVerbose );
      Abc_Print( -2, "\t-Q num : simulator verbosity level [default = %d]\n", nSimulatorVerbose );
      Abc_Print( -2, "\t-S num : SAT solver verbosity level [default = %d]\n", nSatSolverVerbose );
      Abc_Print( -2, "\t-U num : resynthesis verbosity level [default = %d]\n", nResynVerbose );
      Abc_Print( -2, "\t-H num : number of no improvement iterations before each hop [default = %d]\n", nHops );
      Abc_Print( -2, "\t-M num : number of no improvement iterations before each jump [default = %d]\n", nJumps );
      Abc_Print( -2, "\t-l num : number of patterns to relax [default = %d]\n", nRelaxedPatterns );
      Abc_Print( -2, "\t-i str : temporary file prefix [default = \"%s\"]\n", pTemporary? pTemporary: "" );
      Abc_Print( -2, "\t-j str : simulation pattern file [default = \"%s\"]\n", pPattern? pPattern: "" );
      Abc_Print( -2, "\t-k str : custom condition AIG [default = \"%s\"]\n", pCond? pCond: "" );
      Abc_Print( -2, "\t-o str : output file name [default = %s]\n", pOutput? pOutput : "" );
      Abc_Print( -2, "\t-a     : use BDD-based analyzer (CSPF) [default = %s]\n", fUseBddCspf? "yes": "no" );
      Abc_Print( -2, "\t-b     : use BDD-based analyzer (MSPF) [default = %s]\n", fUseBddMspf? "yes": "no" );
      Abc_Print( -2, "\t-d     : ensure deterministic execution [default = %s]\n", fDeterministic? "yes": "no" );
      Abc_Print( -2, "\t-e     : apply \"c2rs; dc2\" after importing changes of partitions [default = %s]\n", fOptOnInsert? "yes": "no" );
      Abc_Print( -2, "\t-g     : discard changes that increased the cost [default = %s]\n", fGreedy? "yes": "no" );
      Abc_Print( -2, "\t-h     : print the command usage\n");

    Abc_Stop();
    return 1;
}


Gia_Man_t *MinimizeTest(Gia_Man_t *pGia, PARAMS_DECL) {
  Gia_Man_t *pNew, *pTmp, *pRes;
  pNew = Gia_ManDup(pGia);
  // pi drop
  int nPis = Gia_ManCiNum(pNew);
  if(nPis > 1) {
    for(int i = 0; i < nPis; i++) {
      for(int k = 0; k < 2; k++) {
        pTmp = Gia_ManDupCofactorVar(pNew, i, k);
        pRes = Gia_ManRrr(pTmp, PARAMS);
        if(Cec_ManVerifyTwo(pTmp, pRes, 1)) {
          Gia_ManStop(pTmp);
        } else {
          Gia_ManStop(pNew);
          pNew = pTmp;
          k = 2;
        }
        Gia_ManStop(pRes);
      }
    }
  }
  // po drop
  int nPos = Gia_ManCoNum(pNew);
  if(nPos > 1) {
    Vec_Int_t * vPos = Vec_IntAlloc(nPos);
    for(int i = 0; i < nPos; i++) {
      Vec_IntClear(vPos);
      for(int j = 0; j < nPos; j++) {
        if(i != j) {
          Vec_IntPush(vPos, j);
        }
      }
      pTmp = Gia_ManDupCones(pNew, Vec_IntArray(vPos), nPos - 1, 1);
      pRes = Gia_ManRrr(pTmp, PARAMS);
      if(Cec_ManVerifyTwo(pTmp, pRes, 1)) {
        Gia_ManStop(pTmp);
      } else {
        Gia_ManStop(pNew);
        pNew = pTmp;
        nPos--;
      }
      Gia_ManStop(pRes);
    }
    Vec_IntFree(vPos);
  }
  // rewrite?
  return pNew;
}
