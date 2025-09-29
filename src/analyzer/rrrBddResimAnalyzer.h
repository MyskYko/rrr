#pragma once

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"
#include "engine/rrrBddManager.h"

namespace rrr {

  template <typename Ntk>
  class BddResimAnalyzer {
  private:
    // aliases
    using lit = int;
    static constexpr lit LitMax = 0xffffffff;

    // pointer to network
    Ntk *pNtk;
    
    // parameters
    int nVerbose;
    bool fSave;
    
    // data
    bool fInitialized;
    int target;
    NewBdd::Man *pBdd;
    lit care;
    std::vector<lit> vFs;

    // backups
    std::vector<BddResimAnalyzer> vBackups;
    
    // updates
    bool fUpdate;
    std::set<int> sUpdates;
    std::vector<bool> vUpdates;

    // stats
    uint64_t nNodesOld;
    uint64_t nNodesAccumulated;
    double durationSimulation;
    double durationPf;
    double durationCheck;
    double durationReorder;
    
    // BDD utils
    void IncRef(lit x) const;
    void DecRef(lit x) const;
    void Assign(lit &x, lit y) const;
    void CopyVec(std::vector<lit> &x, std::vector<lit> const &y) const;
    void DelVec(std::vector<lit> &v) const;
    void CopyVecVec(std::vector<std::vector<lit>> &x, std::vector<std::vector<lit>> const &y) const;
    void DelVecVec(std::vector<std::vector<lit>> &v) const;
    lit  Xor(lit x, lit y) const;

    // callback
    void ActionCallback(Action const &action);

    // simulation
    bool SimulateNode(int id, std::vector<lit> &v) const;
    void Simulate();

    // PF computation
    void ComputeCare(int id);

    // preparation
    void Reset(bool fReuse = false);
    void Initialize();
    
    // save & load
    void Save(int slot);
    void Load(int slot);
    void PopBack();
    
  public:
    // constructors
    BddResimAnalyzer();
    BddResimAnalyzer(Parameter const *pPar);
    ~BddResimAnalyzer();
    void AssignNetwork(Ntk *pNtk_, bool fReuse);

    // checks
    bool CheckRedundancy(int id, int idx);
    bool CheckFeasibility(int id, int fi, bool c);
    
    // summary
    void ResetSummary();
    summary<int> GetStatsSummary() const;
    summary<double> GetTimesSummary() const;
  };
  
  /* {{{ BDD utils */
  
  template <typename Ntk>
  inline void BddResimAnalyzer<Ntk>::IncRef(lit x) const {
    if(x != LitMax) {
      pBdd->IncRef(x);
    }
  }
  
  template <typename Ntk>
  inline void BddResimAnalyzer<Ntk>::DecRef(lit x) const {
    if(x != LitMax) {
      pBdd->DecRef(x);
    }
  }
  
  template <typename Ntk>
  inline void BddResimAnalyzer<Ntk>::Assign(lit &x, lit y) const {
    DecRef(x);
    x = y;
    IncRef(x);
  }

  template <typename Ntk>
  inline void BddResimAnalyzer<Ntk>::CopyVec(std::vector<lit> &x, std::vector<lit> const &y) const {
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
  inline void BddResimAnalyzer<Ntk>::DelVec(std::vector<lit> &v) const {
    for(lit x: v) {
      DecRef(x);
    }
    v.clear();
  }

  template <typename Ntk>
  inline void BddResimAnalyzer<Ntk>::CopyVecVec(std::vector<std::vector<lit>> &x, std::vector<std::vector<lit>> const &y) const {
    for(size_t i = y.size(); i < x.size(); i++) {
      DelVec(x[i]);
    }
    x.resize(y.size());
    for(size_t i = 0; i < y.size(); i++) {
      CopyVec(x[i], y[i]);
    }
  }

  template <typename Ntk>
  inline void BddResimAnalyzer<Ntk>::DelVecVec(std::vector<std::vector<lit>> &v) const {
    for(size_t i = 0; i < v.size(); i++) {
      DelVec(v[i]);
    }
    v.clear();
  }
  
  template <typename Ntk>
  inline typename BddResimAnalyzer<Ntk>::lit BddResimAnalyzer<Ntk>::Xor(lit x, lit y) const {
    lit f = pBdd->And(x, pBdd->LitNot(y));
    pBdd->IncRef(f);
    lit g = pBdd->And(pBdd->LitNot(x), y);
    pBdd->IncRef(g);
    lit r = pBdd->Or(f, g);
    pBdd->DecRef(f);
    pBdd->DecRef(g);
    return r;
  }
  
  /* }}} */

  /* {{{ Callback */

  template <typename Ntk>
  void BddResimAnalyzer<Ntk>::ActionCallback(Action const &action) {
    switch(action.type) {
    case REMOVE_FANIN:
      assert(fInitialized);
      if(action.id == target) {
        fUpdate = true;
      } else {
        sUpdates.insert(action.id);
      }
      break;
    case REMOVE_UNUSED:
      Assign(vFs[action.id], LitMax);
      break;
    case REMOVE_BUFFER:
    case REMOVE_CONST:
      if(fInitialized) {
        if(action.id == target) {
          if(fUpdate) {
            for(int fo: action.vFanouts) {
              sUpdates.insert(fo);
            }
            fUpdate = false;
          }
          target = -1;
        } else {
          if(sUpdates.count(action.id)) {
            sUpdates.erase(action.id);
            for(int fo: action.vFanouts) {
              sUpdates.insert(fo);
            }
          }
        }
      }
      Assign(vFs[action.id], LitMax);
      break;
    case ADD_FANIN:
      assert(fInitialized);
      if(action.id == target) {
        fUpdate = true;
      } else {
        sUpdates.insert(action.id);
      }
      break;
    case TRIVIAL_COLLAPSE:
      Assign(vFs[action.fi], LitMax);
      break;
    case TRIVIAL_DECOMPOSE:
      if(fInitialized) {
        vFs.resize(pNtk->GetNumNodes(), LitMax);
        vUpdates.resize(pNtk->GetNumNodes());
        SimulateNode(action.fi, vFs);
        // time of this simulation is not measured for simplicity sake
      }
      break;
    case SORT_FANINS:
      break;
    case READ:
      Reset(true);
      break;
    case SAVE:
      if(fSave) {
        Save(action.idx);
      }
      break;
    case LOAD:
      if(fSave) {
        Load(action.idx);
      } else {
        Reset(true);
      }
      break;
    case POP_BACK:
      if(fSave) {
        PopBack();
      }
      break;
    default:
      assert(0);
    }
  }
  
  /* }}} */

  /* {{{ Simulation */
  
  template <typename Ntk>
  inline bool BddResimAnalyzer<Ntk>::SimulateNode(int id, std::vector<lit> &v) const {
    if(nVerbose) {
      std::cout << "simulating node " << id << std::endl;
    }
    lit x = pBdd->Const1();
    IncRef(x);
    pNtk->ForEachFanin(id, [&](int fi, bool c) {
      Assign(x, pBdd->And(x, pBdd->LitNotCond(v[fi], c)));
    });
    if(pBdd->LitIsEq(x, v[id])) {
      DecRef(x);
      return false;
    }
    Assign(v[id], x);
    DecRef(x);
    return true;
  }
  
  template <typename Ntk>
  void BddResimAnalyzer<Ntk>::Simulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "symbolic simulation with BDD" << std::endl;
    }
    pNtk->ForEachInt([&](int id) {
      if(vUpdates[id]) {
        if(SimulateNode(id, vFs)) {
          pNtk->ForEachFanout(id, false, [&](int fo) {
            vUpdates[fo] = true;
          });
        }
        vUpdates[id] = false;
      }
    });
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }

  /* }}} */

  /* {{{ MSPF computation */

  template <typename pNtk>
  inline void BddResimAnalyzer<pNtk>::ComputeCare(int id) {
    if(sUpdates.empty() && id == target) {
      return;
    }
    if(fUpdate) {
      sUpdates.insert(target);
      fUpdate = false;
    }
    if(!sUpdates.empty()) {
      for(int id: sUpdates) {
        vUpdates[id] = true;
      }
      Simulate();
      sUpdates.clear();
    }
    target = id;
    time_point timeStart = GetCurrentTime();
    std::vector<lit> v;
    CopyVec(v, vFs);
    v[id] = pBdd->LitNot(v[id]);
    pNtk->ForEachTfoUpdate(id, false, [&](int fo) {
      return SimulateNode(fo, v);
    });
    lit x = pBdd->Const1();
    IncRef(x);
    pNtk->ForEachPoDriver([&](int fi) {
      lit y = Xor(vFs[fi], v[fi]);
      IncRef(y);
      Assign(x, pBdd->And(x, pBdd->LitNot(y)));
      DecRef(y);
    });
    DelVec(v);
    durationPf += Duration(timeStart, GetCurrentTime());
    Assign(care, pBdd->LitNot(x));
    DecRef(x);
  }

  /* }}} */

  /* {{{ Preparation */

  template <typename Ntk>
  void BddResimAnalyzer<Ntk>::Reset(bool fReuse) {
    while(!vBackups.empty()) {
      PopBack();
    }
    target = -1;
    DelVec(vFs);
    Assign(care, LitMax),
    fInitialized = false;
    fUpdate = false;
    sUpdates.clear();
    vUpdates.clear();
    if(!fReuse) {
      nNodesOld = 0;
      if(pBdd) {
        nNodesAccumulated += pBdd->GetNumTotalCreatedNodes();
      }
      delete pBdd;
      pBdd = NULL;
    }
  }

  template <typename Ntk>
  void BddResimAnalyzer<Ntk>::Initialize() {
    bool fUseReo = false;
    if(!pBdd) {
      NewBdd::Param Par;
      pBdd = new NewBdd::Man(pNtk->GetNumPis(), Par);
      fUseReo = true;
    }
    assert(pBdd->GetNumVars() == pNtk->GetNumPis());
    vFs.resize(pNtk->GetNumNodes(), LitMax);
    Assign(vFs[0], pBdd->Const0());
    int idx = 0;
    pNtk->ForEachPi([&](int id) {
      Assign(vFs[id], pBdd->IthVar(idx));
      idx++;
    });
    vUpdates.resize(pNtk->GetNumNodes());
    pNtk->ForEachInt([&](int id) {
      vUpdates[id] = true;
    });
    Simulate();
    if(fUseReo) {
      time_point timeStart = GetCurrentTime();
      pBdd->Reorder();
      pBdd->TurnOffReo();
      durationReorder += Duration(timeStart, GetCurrentTime());
    }
    fInitialized = true;
  }

  /* }}} */
  
  /* {{{ Save & load */

  template <typename Ntk>
  void BddResimAnalyzer<Ntk>::Save(int slot) {
    if(slot >= int_size(vBackups)) {
      vBackups.resize(slot + 1);
    }
    vBackups[slot].fInitialized = fInitialized;
    if(!fInitialized) {
      return;
    }
    if(sUpdates.empty()) {
      vBackups[slot].target = target;
      Assign(vBackups[slot].care, care);
    } else {
      vBackups[slot].target = -1;
      Assign(vBackups[slot].care, care);
    }
    if(fUpdate) {
      sUpdates.insert(target);
      fUpdate = false;
    }
    if(!sUpdates.empty()) {
      for(int id: sUpdates) {
        vUpdates[id] = true;
      }
      Simulate();
      sUpdates.clear();
    }
    target = vBackups[slot].target; // assigned to -1 when careset needs updating
    CopyVec(vBackups[slot].vFs, vFs);
  }

  template <typename Ntk>
  void BddResimAnalyzer<Ntk>::Load(int slot) {
    assert(slot < int_size(vBackups));
    if(!vBackups[slot].fInitialized) {
      Reset(true);
      return;
    }
    target = vBackups[slot].target;
    Assign(care, vBackups[slot].care);
    CopyVec(vFs, vBackups[slot].vFs);
    fUpdate = false;
    sUpdates.clear();
    vUpdates.clear();
    vUpdates.resize(pNtk->GetNumNodes());
  }

  template <typename Ntk>
  void BddResimAnalyzer<Ntk>::PopBack() {
    assert(!vBackups.empty());
    DelVec(vBackups.back().vFs);
    Assign(vBackups.back().care, LitMax);
    vBackups.pop_back();
  }

  /* }}} */
  
  /* {{{ Constructor */

  template <typename Ntk>
  BddResimAnalyzer<Ntk>::BddResimAnalyzer() :
    pNtk(NULL),
    nVerbose(0),
    fSave(false),
    fInitialized(false),
    target(-1),
    pBdd(NULL),
    care(LitMax),
    fUpdate(false) {
    ResetSummary();
  }
  
  template <typename Ntk>
  BddResimAnalyzer<Ntk>::BddResimAnalyzer(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nAnalyzerVerbose),
    fSave(pPar->fSave),
    fInitialized(false),
    target(-1),
    pBdd(NULL),
    care(LitMax),
    fUpdate(false) {
    ResetSummary();
  }

  template <typename Ntk>
  BddResimAnalyzer<Ntk>::~BddResimAnalyzer() {
    Reset();
  }

  template <typename Ntk>
  void BddResimAnalyzer<Ntk>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    Reset(fReuse);
    pNtk = pNtk_;
    pNtk->AddCallback(std::bind(&BddResimAnalyzer<Ntk>::ActionCallback, this, std::placeholders::_1));
  }
  
  /* }}} */

  /* {{{ Checks */
  
  template <typename Ntk>
  bool BddResimAnalyzer<Ntk>::CheckRedundancy(int id, int idx) {
    if(!fInitialized) {
      Initialize();
    }
    ComputeCare(id);
    lit x = care;
    IncRef(x);
    time_point timeStart = GetCurrentTime();
    bool fRedundant = false;
    switch(pNtk->GetNodeType(id)) {
    case AND: {
      pNtk->ForEachFaninIdx(id, [&](int idx2, int fi, bool c) {
        if(idx == idx2) {
          return;
        }
        Assign(x, pBdd->And(x, pBdd->LitNotCond(vFs[fi], c)));
      });
      int fi = pNtk->GetFanin(id, idx);
      bool c = pNtk->GetCompl(id, idx);
      Assign(x, pBdd->And(x, pBdd->LitNotCond(vFs[fi], !c)));
      fRedundant = pBdd->IsConst0(x);
      break;
    }
    default:
      assert(0);
    }
    DecRef(x);
    if(nVerbose) {
      if(fRedundant) {
        std::cout << "node " << id << " fanin " << (pNtk->GetCompl(id, idx)? "!": "") << pNtk->GetFanin(id, idx) << " index " << idx << " is redundant" << std::endl;
      } else {
        std::cout << "node " << id << " fanin " << (pNtk->GetCompl(id, idx)? "!": "") << pNtk->GetFanin(id, idx) << " index " << idx << " is NOT redundant" << std::endl;
      }
    }
    durationCheck += Duration(timeStart, GetCurrentTime());
    return fRedundant;
  }
  
  template <typename Ntk>
  bool BddResimAnalyzer<Ntk>::CheckFeasibility(int id, int fi, bool c) {
    if(!fInitialized) {
      Initialize();
    }
    ComputeCare(id);
    lit x = care;
    IncRef(x);
    time_point timeStart = GetCurrentTime();
    bool fFeasible = false;
    switch(pNtk->GetNodeType(id)) {
    case AND: {
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        Assign(x, pBdd->And(x, pBdd->LitNotCond(vFs[fi], c)));
      });
      Assign(x, pBdd->And(x, pBdd->LitNotCond(vFs[fi], !c)));
      fFeasible = pBdd->IsConst0(x);
      break;
    }
    default:
      assert(0);
    }
    DecRef(x);
    if(nVerbose) {
      if(fFeasible) {
        std::cout << "node " << id << " fanin " << (c? "!": "") << fi << " is feasible" << std::endl;
      } else {
        std::cout << "node " << id << " fanin " << (c? "!": "") << fi << " is NOT feasible" << std::endl;
      }
    }
    durationCheck += Duration(timeStart, GetCurrentTime());
    return fFeasible;
  }

  /* }}} */
  
  /* {{{ Summary */

  template <typename Ntk>
  void BddResimAnalyzer<Ntk>::ResetSummary() {
    if(pBdd) {
      nNodesOld = pBdd->GetNumTotalCreatedNodes();
    } else {
      nNodesOld = 0;
    }
    nNodesAccumulated = 0;
    durationSimulation = 0;
    durationPf = 0;
    durationCheck = 0;
    durationReorder = 0;
  }
  
  template <typename Ntk>
  summary<int> BddResimAnalyzer<Ntk>::GetStatsSummary() const {
    summary<int> v;
    v.emplace_back("bdd node", pBdd->GetNumTotalCreatedNodes() - nNodesOld + nNodesAccumulated);
    return v;
  }
  
  template <typename Ntk>
  summary<double> BddResimAnalyzer<Ntk>::GetTimesSummary() const {
    summary<double> v;
    v.emplace_back("bdd symbolic simulation", durationSimulation);
    v.emplace_back("bdd care computation", durationPf);
    v.emplace_back("bdd check", durationCheck);
    v.emplace_back("bdd reorder", durationReorder);
    return v;
  }

  /* }}} */
  
}
