#pragma once

#include <iostream>
#include <vector>

#include <aig/gia/giaNewBdd.h>

#include "rrrParameter.h"
#include "rrrTypes.h"


namespace rrr {

  template <typename Ntk>
  class BddAnalyzer {
  private:
    using lit = int;
    static constexpr lit LitMax = 0xffffffff;
    static const bool fResim = false;

    int nVerbose;
    Ntk *pNtk;
    NewBdd::Man *pBdd;
    int target;
    
    std::vector<lit> vFs;
    std::vector<lit> vGs;
    std::vector<std::vector<lit>> vvCs;

    std::vector<bool> vUpdates;
    std::vector<bool> vGUpdates;
    std::vector<bool> vCUpdates;

    std::vector<BddAnalyzer> vBackups;

    void IncRef(lit x) const;
    void DecRef(lit x) const;
    void Assign(lit &x, lit y) const;
    void CopyVec(std::vector<lit> &x, std::vector<lit> const &y) const;
    void DelVec(std::vector<lit> &v) const;
    void CopyVecVec(std::vector<std::vector<lit>> &x, std::vector<std::vector<lit>> const &y) const;
    void DelVecVec(std::vector<std::vector<lit>> &v) const;
    lit  Xor(lit x, lit y) const;

    std::function<void(Action)> CreateActionCallback();

    void Allocate();

    void SimulateNode(int id, std::vector<lit> &v) const;
    void Simulate();
    
    bool ComputeG(int id);
    void ComputeC(int id);
    void CspfNode(int id);
    void Cspf(int id = -1);

    void Save(int slot);
    void Load(int slot);
    void PopBack();

  public:
    BddAnalyzer();
    BddAnalyzer(Ntk *pNtk, Parameter *pPar);
    ~BddAnalyzer();
    void UpdateNetwork(Ntk *pNtk_);

    bool CheckRedundancy(int id, int idx);
    bool CheckFeasibility(int id, int fi, bool c);
  };
  
  /* {{{ Bdd utils */
  
  template <typename Ntk>
  inline void BddAnalyzer<Ntk>::IncRef(lit x) const {
    if(x != LitMax) {
      pBdd->IncRef(x);
    }
  }
  
  template <typename Ntk>
  inline void BddAnalyzer<Ntk>::DecRef(lit x) const {
    if(x != LitMax) {
      pBdd->DecRef(x);
    }
  }
  
  template <typename Ntk>
  inline void BddAnalyzer<Ntk>::Assign(lit &x, lit y) const {
    DecRef(x);
    x = y;
    IncRef(x);
  }

  template <typename Ntk>
  inline void BddAnalyzer<Ntk>::CopyVec(std::vector<lit> &x, std::vector<lit> const &y) const {
    for(size_t i = y.size(); i < x.size(); i++) {
      DecRef(x[i]);
    }
    x.resize(y.size(), LitMax);
    for(size_t i = 0; i < y.size(); i++) {
      if(x[i] != y[i]) {
        DecRef(x[i]);
        x[i] = y[i];
        IncRef(x[i]);
      }
    }
  }

  template <typename Ntk>
  inline void BddAnalyzer<Ntk>::DelVec(std::vector<lit> &v) const {
    for(lit x: v) {
      DecRef(x);
    }
    v.clear();
  }

  template <typename Ntk>
  inline void BddAnalyzer<Ntk>::CopyVecVec(std::vector<std::vector<lit>> &x, std::vector<std::vector<lit>> const &y) const {
    for(size_t i = y.size(); i < x.size(); i++) {
      DelVec(x[i]);
    }
    x.resize(y.size());
    for(size_t i = 0; i < y.size(); i++) {
      CopyVec(x[i], y[i]);
    }
  }

  template <typename Ntk>
  inline void BddAnalyzer<Ntk>::DelVecVec(std::vector<std::vector<lit>> &v) const {
    for(size_t i = 0; i < v.size(); i++) {
      DelVec(v[i]);
    }
    v.clear();
  }
  
  template <typename Ntk>
  inline int BddAnalyzer<Ntk>::Xor(lit x, lit y) const {
    lit f = pBdd->And(x, pBdd->LitNot(y));
    pBdd->IncRef(f);
    lit g = pBdd->And(pBdd->LitNot(x), y);
    pBdd->IncRef(g);
    lit r = pBdd->Or(f, g);
    pBdd->DecRef(f);
    pBdd->DecRef(g);
    return r;
  }
  
  /* }}} Bdd utils end */

  /* {{{ Create action callback */

  template <typename Ntk>
  std::function<void(Action)> BddAnalyzer<Ntk>::CreateActionCallback() {
    return [&](Action action) {
      switch(action.type) {
      case REMOVE_FANIN:
        vUpdates[action.id] = true;
        vGUpdates[action.fi] = true;
        DecRef(vvCs[action.id][action.idx]);
        vvCs[action.id].erase(vvCs[action.id].begin() + action.idx);
        break;
      case REMOVE_UNUSED:
        if(vGUpdates[action.id] || vCUpdates[action.id]) {
          for(int fi: action.vFanins) {
            vGUpdates[fi] = true;
          }
        }
        Assign(vFs[action.id], LitMax);
        Assign(vGs[action.id], LitMax);
        DelVec(vvCs[action.id]);
        break;
      case REMOVE_BUFFER:
        if(vUpdates[action.id]) {
          for(int fo: action.vFanouts) {
            vUpdates[fo] = true;
            vCUpdates[fo] = true;
          }
        }
        if(vGUpdates[action.id] || vCUpdates[action.id]) {
          vGUpdates[action.fi] = true;
        }
        Assign(vFs[action.id], LitMax);
        Assign(vGs[action.id], LitMax);
        DelVec(vvCs[action.id]);
        break;
      case REMOVE_CONST:
        if(vUpdates[action.id]) {
          for(int fo: action.vFanouts) {
            vUpdates[fo] = true;
            vCUpdates[fo] = true;
          }
        }
        for(int fi: action.vFanins) {
          vGUpdates[fi] = true;
        }
        Assign(vFs[action.id], LitMax);
        Assign(vGs[action.id], LitMax);
        DelVec(vvCs[action.id]);
        break;
      case ADD_FANIN:
        vUpdates[action.id] = true;
        vCUpdates[action.id] = true;
        vvCs[action.id].insert(vvCs[action.id].begin() + action.idx, LitMax);
        break;
      case TRIVIAL_COLLAPSE: {
        if(vGUpdates[action.fi] || vCUpdates[action.fi]) {
          vCUpdates[action.id] = true;
        }
        std::vector<lit>::iterator it = vvCs[action.id].begin() + action.idx;
        DecRef(*it);
        it = vvCs[action.id].erase(it);
        vvCs[action.id].insert(it,  vvCs[action.fi].begin(), vvCs[action.fi].end());
        vvCs[action.fi].clear();
        Assign(vFs[action.fi], LitMax);
        Assign(vGs[action.fi], LitMax);
        break;
      }
      case TRIVIAL_DECOMPOSE: {
        Allocate();
        SimulateNode(action.fi, vFs);
        assert(vGs[action.fi] == LitMax);
        Assign(vGs[action.fi], vGs[action.id]);
        std::vector<lit>::iterator it = vvCs[action.id].begin() + action.idx;
        assert(vvCs[action.fi].empty());
        vvCs[action.fi].insert(vvCs[action.fi].begin(), it, vvCs[action.id].end());
        vvCs[action.id].erase(it, vvCs[action.id].end());
        assert(vvCs[action.id].size() == action.idx);
        vvCs[action.id].resize(action.idx + 1, LitMax);
        Assign(vvCs[action.id][action.idx], vGs[action.fi]);
        vUpdates[action.fi] = false;
        vGUpdates[action.fi] = false;
        vCUpdates[action.fi] = vCUpdates[action.id];
        break;
      }
      case SAVE:
        Save(action.id);
        break;
      case LOAD:
        Load(action.id);
        break;
      case POP_BACK:
        PopBack();
        break;
      default:
        assert(0);
      }
    };
  }
  
  /* }}} Create action callback end */

  /* {{{ Allocation */

  template <typename Ntk>
  void BddAnalyzer<Ntk>::Allocate() {
    int nNodes = pNtk->GetNumNodes();
    vFs.resize(nNodes, LitMax);
    vGs.resize(nNodes, LitMax);
    vvCs.resize(nNodes);
    vUpdates.resize(nNodes);
    vGUpdates.resize(nNodes);
    vCUpdates.resize(nNodes);
  }

  /* }}} Allocation end */
  
  /* {{{ Simulation */
  
  template <typename Ntk>
  inline void BddAnalyzer<Ntk>::SimulateNode(int id, std::vector<lit> &v) const {
    if(nVerbose) {
      std::cout << "simulating node " << id << std::endl;
    }
    Assign(v[id], pBdd->Const1());
    pNtk->ForEachFanin(id, [&](int fi, bool c) {
      Assign(v[id], pBdd->And(v[id], pBdd->LitNotCond(v[fi], c)));
    });
  }
  
  template <typename Ntk>
  void BddAnalyzer<Ntk>::Simulate() {
    pNtk->ForEachInt([&](int id) {
      if(vUpdates[id]) {
        lit x = vFs[id];
        IncRef(x);
        SimulateNode(id, vFs);
        DecRef(x);
        if(!pBdd->LitIsEq(x, vFs[id])) {
          pNtk->ForEachFanout(id, [&](int fo, bool c) {
            vUpdates[fo] = true;
            vCUpdates[fo] = true;
          });
        }
        vUpdates[id] = false;
      }
    });
  }

  /* }}} Simulation end */

  /* {{{ Cspf */
  
  template <typename Ntk>
  inline bool BddAnalyzer<Ntk>::ComputeG(int id) {
    if(pNtk->GetNumFanouts(id) == 0) {
      if(pBdd->IsConst1(vGs[id])) {
        return false;
      }
      Assign(vGs[id], pBdd->Const1());
      return true;
    }
    lit x = pBdd->Const1();
    IncRef(x);
    pNtk->ForEachFanoutIdx(id, [&](int fo, bool c, int idx) {
      Assign(x, pBdd->And(x, vvCs[fo][idx]));
    });
    if(pBdd->LitIsEq(vGs[id], x)) {
      DecRef(x);
      return false;
    }
    Assign(vGs[id], x);
    DecRef(x);
    return true;
  }

  template <typename Ntk>
  inline void BddAnalyzer<Ntk>::ComputeC(int id) {
    int nFanins = pNtk->GetNumFanins(id);
    assert(vvCs[id].size() == nFanins);
    if(pBdd->IsConst1(vGs[id])) {
      for(int idx = 0; idx < nFanins; idx++) {
        if(!pBdd->IsConst1(vvCs[id][idx])) {
          Assign(vvCs[id][idx], pBdd->Const1());
          int fi = pNtk->GetFanin(id, idx);
          vGUpdates[fi] = true;
        }
      }
      return;
    }
    for(int idx = 0; idx < nFanins; idx++) {
      lit x = pBdd->Const1();
      IncRef(x);
      for(unsigned idx2 = idx + 1; idx2 < nFanins; idx2++) {
        int fi = pNtk->GetFanin(id, idx2);
        bool c = pNtk->GetCompl(id, idx2);
        Assign(x, pBdd->And(x, pBdd->LitNotCond(vFs[fi], c)));
      }
      Assign(x, pBdd->Or(pBdd->LitNot(x), vGs[id]));
      if(!pBdd->LitIsEq(vvCs[id][idx], x)) {
        Assign(vvCs[id][idx], x);
        int fi = pNtk->GetFanin(id, idx);
        vGUpdates[fi] = true;
      }
      DecRef(x);
    }
  }

  template <typename Ntk>
  void BddAnalyzer<Ntk>::CspfNode(int id) {
    if(!vGUpdates[id] && !vCUpdates[id]) {
      return;
    }
    if(vGUpdates[id]) {
      if(nVerbose) {
        std::cout << "computing node " << id << " G " << std::endl;
      }
      if(ComputeG(id)) {
        vCUpdates[id] = true;
      }
      vGUpdates[id] = false;
    }
    if(vCUpdates[id]) {
      if(nVerbose) {
        std::cout << "computing node " << id << " C " << std::endl;
      }
      ComputeC(id);
      vCUpdates[id] = false;
    }
  }
  
  template <typename Ntk>
  void BddAnalyzer<Ntk>::Cspf(int id) {
    if(id != -1) {
      pNtk->ForEachTfoReverse(id, false, [&](int fo) {
        CspfNode(fo);
      });
      CspfNode(id);
    } else {
      pNtk->ForEachIntReverse([&](int id) {
        CspfNode(id);
      });
    }
  }

  /* }}} Cspf end */

  /* {{{ Save & load */

  template <typename Ntk>
  void BddAnalyzer<Ntk>::Save(int slot) {
    if(slot >= vBackups.size()) {
      vBackups.resize(slot + 1);
    }
    vBackups[slot].target = target;
    CopyVec(vBackups[slot].vFs, vFs);
    CopyVec(vBackups[slot].vGs, vGs);
    CopyVecVec(vBackups[slot].vvCs, vvCs);
    vBackups[slot].vUpdates = vUpdates;
    vBackups[slot].vGUpdates = vGUpdates;
    vBackups[slot].vCUpdates = vCUpdates;
  }

  template <typename Ntk>
  void BddAnalyzer<Ntk>::Load(int slot) {
    assert(slot < vBackups.size());
    target = vBackups[slot].target;
    CopyVec(vFs, vBackups[slot].vFs);
    CopyVec(vGs, vBackups[slot].vGs);
    CopyVecVec(vvCs, vBackups[slot].vvCs);
    vUpdates = vBackups[slot].vUpdates;
    vGUpdates = vBackups[slot].vGUpdates;
    vCUpdates = vBackups[slot].vCUpdates;
  }

  template <typename Ntk>
  void BddAnalyzer<Ntk>::PopBack() {
    assert(!vBackups.empty());
    DelVec(vBackups.back().vFs);
    DelVec(vBackups.back().vGs);
    DelVecVec(vBackups.back().vvCs);
    vBackups.pop_back();
  }

  /* }}} Save & load end */
  
  /* {{{ Constructor */

  template <typename Ntk>
  BddAnalyzer<Ntk>::BddAnalyzer() :
    nVerbose(0),
    pNtk(NULL),
    pBdd(NULL),
    target(-1) {
  }
  
  template <typename Ntk>
  BddAnalyzer<Ntk>::BddAnalyzer(Ntk *pNtk, Parameter *pPar) :
    nVerbose(pPar->nAnalyzerVerbose),
    pNtk(pNtk),
    target(-1) {
    NewBdd::Param Par;
    Par.nObjsMaxLog = 25;
    Par.nCacheMaxLog = 20;
    Par.fCountOnes = true;
    Par.nGbc = 1;
    Par.nReo = 4000;
    pBdd = new NewBdd::Man(pNtk->GetNumPis(), Par);
    Allocate();
    Assign(vFs[0], pBdd->Const0());
    int idx = 0;
    pNtk->ForEachPi([&](int id) {
      Assign(vFs[id], pBdd->IthVar(idx));
      idx++;
    });
    pNtk->ForEachInt([&](int id) {
      vUpdates[id] = true;
    });
    Simulate();
    pBdd->Reorder();
    pBdd->TurnOffReo();
    pNtk->ForEachInt([&](int id) {
      vvCs[id].resize(pNtk->GetNumFanins(id), LitMax);
    });
    pNtk->ForEachPo([&](int id) {
      vvCs[id].resize(1, LitMax);
      Assign(vvCs[id][0], pBdd->Const0());
      int fi = pNtk->GetFanin(id, 0);
      vGUpdates[fi]  = true;
    });
    pNtk->AddCallback(CreateActionCallback());
  }

  template <typename Ntk>
  void BddAnalyzer<Ntk>::UpdateNetwork(Ntk *pNtk_) {
    pNtk = pNtk_;
    assert(0);
  }
  
  template <typename Ntk>
  BddAnalyzer<Ntk>::~BddAnalyzer() {
    while(!vBackups.empty()) {
      PopBack();
    }
    DelVec(vFs);
    DelVec(vGs);
    DelVecVec(vvCs);
    delete pBdd;
  }

  /* }}} Constructor end */

  /* {{{ Perform checks */
  
  template <typename Ntk>
  bool BddAnalyzer<Ntk>::CheckRedundancy(int id, int idx) {
    if(fResim) {
      Simulate();
    }
    Cspf(id);
    switch(pNtk->GetNodeType(id)) {
    case AND: {
      int fi = pNtk->GetFanin(id, idx);
      bool c = pNtk->GetCompl(id, idx);
      return pBdd->IsConst1(pBdd->Or(pBdd->LitNotCond(vFs[fi], c), vvCs[id][idx]));
    }
    default:
      assert(0);
    }
    return false;
  }
  
  template <typename Ntk>
  bool BddAnalyzer<Ntk>::CheckFeasibility(int id, int fi, bool c) {
    if(target != id) {
      Simulate();
      target = id;
    }
    Cspf(id);
    //std::cout << "f = " << vFs[id] << ", g = " << vGs[id] << ", h = " << vFs[fi] << std::endl;
    lit x = pBdd->Or(pBdd->LitNot(vFs[id]), vGs[id]);
    IncRef(x);
    if(pBdd->IsConst1(pBdd->Or(x, pBdd->LitNotCond(vFs[fi], c)))) {
      DecRef(x);
      return true;
    }
    DecRef(x);
    return false;
  }

  /* }}} Perform check end */
  
}
