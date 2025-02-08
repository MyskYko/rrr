#include "aig/gia/gia.h"

#include "rrr.h"

template <typename Ntk>
Ntk *Aig2Ntk(Gia_Man_t *pGia) {
  int i;
  Gia_Obj_t *pObj;
  Ntk *pNtk = new Ntk(Gia_ManObjNum(pGia));
  Gia_ManConst0(pGia)->Value = pNtk->GetConst0();
  Gia_ManForEachObj1(pGia, pObj, i) {
    if(Gia_ObjIsCi(pObj)) {
      pObj->Value = pNtk->AddPi();
    } else if(Gia_ObjIsCo(pObj)) {
      pNtk->AddPo(Gia_ObjFanin0(pObj)->Value, Gia_ObjFaninC0(pObj));
    } else {
      // TODO: support XOR (and BUF and MUX?), maybe create another function
      pObj->Value = pNtk->AddAnd(Gia_ObjFanin0(pObj)->Value, Gia_ObjFanin1(pObj)->Value, Gia_ObjFaninC0(pObj), Gia_ObjFaninC1(pObj));
    }
  }
  return pNtk;
}

template <typename Ntk>
Gia_Man_t *Ntk2Aig(Ntk *pNtk) {
  Gia_Man_t *pGia = Gia_ManStart(pNtk->GetNumNodes());
  Gia_ManHashStart(pGia);
  std::vector<int> v(pNtk->GetNumNodes());
  v[0] = Gia_ManConst0Lit();
  pNtk->ForEachPi([&](int id) {
    v[id] = Gia_ManAppendCi(pGia);
  });
  pNtk->ForEachInt([&](int id) {
    assert(pNtk->GetNodeType(id) == rrr::AND);
    int x = -1;
    pNtk->ForEachFanin(id, [&](int fi, bool c) {
      if(x == -1) {
        x = Abc_LitNotCond(v[fi], c);
      } else {
        x = Gia_ManHashAnd(pGia, x, Abc_LitNotCond(v[fi], c));
      }
    });
    if(x == -1) {
      x = Abc_LitNot(v[0]);
    }
    v[id] = x;
  });
  pNtk->ForEachPoDriver([&](int fi, bool c) {
    Gia_ManAppendCo(pGia, Abc_LitNotCond(v[fi], c));
  });
  Gia_ManHashStop(pGia);
  return pGia;  
}

extern "C"
Gia_Man_t *Gia_ManRrr(Gia_Man_t *pGia, int iSeed, int nWords, int nTimeout, int nSchedulerVerbose, int nOptimizerVerbose, int nAnalyzerVerbose, int nSimulatorVerbose, int nSatSolverVerbose, int fUseBddCspf, int fUseBddMspf, int nConflictLimit) {
  rrr::AndNetwork *pNtk = Aig2Ntk<rrr::AndNetwork>(pGia);
  rrr::Parameter Par;
  Par.iSeed = iSeed;
  Par.nWords = nWords;
  Par.nTimeout = nTimeout;
  Par.nSchedulerVerbose = nSchedulerVerbose;
  Par.nOptimizerVerbose = nOptimizerVerbose;
  Par.nAnalyzerVerbose = nAnalyzerVerbose;
  Par.nSimulatorVerbose = nSimulatorVerbose;
  Par.nSatSolverVerbose = nSatSolverVerbose;
  Par.fUseBddCspf = fUseBddCspf;
  Par.fUseBddMspf = fUseBddMspf;
  Par.nConflictLimit = nConflictLimit;
  rrr::Perform(pNtk, &Par);
  return Ntk2Aig(pNtk);
}
