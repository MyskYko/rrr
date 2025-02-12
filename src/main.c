#include <base/abc/abc.h>
#include <base/main/main.h>
#include <base/main/mainInt.h>
#include <aig/gia/giaAig.h>
// #include <misc/st/st.h>
// #include <map/mio/mio.h>
// #include <map/amap/amap.h>
#include <proof/cec/cec.h>


#define PARAMS iSeed, nWords, nTimeout, nSchedulerVerbose, nOptimizerVerbose, nAnalyzerVerbose, nSimulatorVerbose, nSatSolverVerbose, fUseBddCspf, fUseBddMspf, nConflictLimit, nSortType, nOptimizerFlow, nSchedulerFlow
#define PARAMS_DEF int iSeed = 0, nWords = 10, nTimeout = 0, nSchedulerVerbose = 1, nOptimizerVerbose = 0, nAnalyzerVerbose = 0, nSimulatorVerbose = 0, nSatSolverVerbose = 0, fUseBddCspf = 0, fUseBddMspf = 0, nConflictLimit = 0, nSortType = 0, nOptimizerFlow = 0, nSchedulerFlow = 0
#define PARAMS_DECL int iSeed, int nWords, int nTimeout, int nSchedulerVerbose, int nOptimizerVerbose, int nAnalyzerVerbose, int nSimulatorVerbose, int nSatSolverVerbose, int fUseBddCspf, int fUseBddMspf, int nConflictLimit, int nSortType, int nOptimizerFlow, int nSchedulerFlow


extern Gia_Man_t *Gia_ManRrr(Gia_Man_t *pGia, PARAMS_DECL);
Gia_Man_t *MinimizeTest(Gia_Man_t *pGia, PARAMS_DECL);


int main(int argc, char **argv) {
  Abc_Start();
  
  int c;
  PARAMS_DEF;
  int fCore = 0;
  Extra_UtilGetoptReset();
  while ( ( c = Extra_UtilGetopt( argc, argv, "XYRWTCGSOAPQabch" ) ) != EOF )
  {
      switch ( c )
      {
      case 'X':
          nOptimizerFlow = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'Y':
          nSchedulerFlow = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'R':
          iSeed = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'W':
          nWords = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'T':
          nTimeout = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'C':
          nConflictLimit = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'G':
          nSortType = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'S':
          nSchedulerVerbose = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'O':
          nOptimizerVerbose = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'A':
          nAnalyzerVerbose = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'P':
          nSimulatorVerbose = atoi(argv[globalUtilOptind]);
          globalUtilOptind++;
          break;
      case 'Q':
          nSatSolverVerbose = atoi(argv[globalUtilOptind]);
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
      case 'h':
          goto usage;
      default:
          goto usage;
      }
  }
  
  if(argc != globalUtilOptind + 1) {
    goto usage;
  }
  
  char * fname = argv[globalUtilOptind];
  Gia_Man_t *pGia = Gia_AigerRead(fname, 0, 0, 0);
  printf("start: %d nodes\n", Gia_ManAndNum(pGia));

  Gia_Man_t *pNew = Gia_ManRrr(pGia, PARAMS);
  printf("end:   %d nodes\n", Gia_ManAndNum(pNew));

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

  //Gia_AigerWrite(pNew, "out.aig", 0, 0, 0);
  
  Gia_ManStop(pGia);
  Gia_ManStop(pNew);
  
  return 0;

usage:
      Abc_Print( -2, "usage: rrr [-XYRWTCGSOAPQ num] [-abh]\n" );
      Abc_Print( -2, "\t        perform optimization\n" );
      Abc_Print( -2, "\t-X num : method [default = %d]\n", nOptimizerFlow );
      Abc_Print( -2, "\t                0: single-add resub\n" );
      Abc_Print( -2, "\t                1: multi-add resub\n" );
      Abc_Print( -2, "\t                2: repeat 0 and 1\n" );
      Abc_Print( -2, "\t-Y num : flow [default = %d]\n", nSchedulerFlow );
      Abc_Print( -2, "\t                0: apply method once\n" );
      Abc_Print( -2, "\t                1: iterate like transtoch\n" );
      Abc_Print( -2, "\t                2: iterate like deepsyn\n" );
      Abc_Print( -2, "\t-R num : random number generator seed [default = %d]\n", iSeed );
      Abc_Print( -2, "\t-W num : number of simulation words [default = %d]\n", nWords );
      Abc_Print( -2, "\t-T num : timeout in seconds (0 = no timeout) [default = %d]\n", nTimeout );
      Abc_Print( -2, "\t-C num : conflict limit [default = %d]\n", nConflictLimit );
      Abc_Print( -2, "\t-G num : fanin cost function [default = %d]\n", nSortType );
      Abc_Print( -2, "\t-S num : scheduler verbosity level [default = %d]\n", nSchedulerVerbose );
      Abc_Print( -2, "\t-O num : optimizer verbosity level [default = %d]\n", nOptimizerVerbose );
      Abc_Print( -2, "\t-A num : analyzer verbosity level [default = %d]\n", nAnalyzerVerbose );
      Abc_Print( -2, "\t-P num : simulator verbosity level [default = %d]\n", nSimulatorVerbose );
      Abc_Print( -2, "\t-Q num : SAT solver verbosity level [default = %d]\n", nSatSolverVerbose );
      Abc_Print( -2, "\t-a     : use BDD-based analyzer (CSPF) [default = %s]\n", fUseBddCspf? "yes": "no" );
      Abc_Print( -2, "\t-b     : use BDD-based analyzer (MSPF) [default = %s]\n", fUseBddMspf? "yes": "no" );
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
