#pragma once

#include <algorithm>
#include <random>
#include <bitset>

#include "rrrParameter.h"
#include "rrrUtils.h"
#include "rrrPattern.h"

namespace rrr {

  template <typename Ntk>
  class Simulator2 {
  private:
    // aliases
    using word = unsigned long long;
    using itr = std::vector<word>::iterator;
    using citr = std::vector<word>::const_iterator;
    static constexpr word one = 0xffffffffffffffff;
    static constexpr bool fKeepStimuli = true;

    // pointer to network
    Ntk *pNtk;
    
    // parameters
    int nVerbose;
    int nWords;
    int nStimuli;

    // data
    bool fGenerated;
    bool fInitialized;
    int target; // node for which the careset has been computed
    std::vector<word> vValues;
    std::vector<word> vValues2; // simulation with an inverter
    std::vector<word> care; // careset
    std::vector<word> tmp;

    // partial cex
    int iPivot;
    std::vector<word> vAssignedStimuli;

    // updates
    bool fUpdate;
    std::set<int> sUpdates;

    // stats
    int nCex;
    int nDiscarded;
    int nPackedCountOld;
    std::vector<int> vPackedCount;
    std::vector<int> vPackedCountEvicted;
    double durationSimulation;
    double durationCare;
    
    // vector computations
    void Clear(int n, itr x) const;
    void Fill(int n, itr x) const;
    void Copy(int n, itr dst, citr src, bool c) const;
    void And(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const;
    void Xor(int n, itr dst, citr src0, citr src1, bool c) const;
    bool IsZero(int n, citr x) const;
    bool IsEq(int n, citr x, citr y) const;
    void Print(int n, citr x) const;

    // callback
    void ActionCallback(Action const &action);

    // simulation
    void SimulateNode(std::vector<word> &v, int id, int to_negate = -1);
    bool ResimulateNode(std::vector<word> &v, int id, int to_negate = -1);
    void SimulateOneWordNode(std::vector<word> &v, int id, int offset, int to_negate = -1);
    void SimulatePartNode(std::vector<word> &v, int id, int n, int offset, int to_negate = -1);
    void Simulate();
    void Resimulate();
    void SimulateOneWord(int offset);

    // generate stimuli
    void GenerateRandomStimuli();
    void ReadStimuli(Pattern *pPat);

    // careset computation
    void ComputeCare(int id);
    void ComputeCarePart(int nWords_, int offset);

    // preparation
    void Initialize();

  public:
    // constructors
    Simulator2();
    Simulator2(Parameter const *pPar);
    void AssignNetwork(Ntk *pNtk_, bool fReuse);

    // checks
    bool CheckRedundancy(int id, int idx);
    bool CheckFeasibility(int id, int fi, bool c);

    // cex
    void AddCex(std::vector<VarValue> const &vCex);

    // summary
    void ResetSummary();
    summary<int> GetStatsSummary() const;
    summary<double> GetTimesSummary() const;
  };


  /* {{{ Vector computations */
  
  template <typename Ntk>
  inline void Simulator2<Ntk>::Clear(int n, itr x) const {
    std::fill(x, x + n, 0);
  }

  template <typename Ntk>
  inline void Simulator2<Ntk>::Fill(int n, itr x) const {
    std::fill(x, x + n, one);
  }
  
  template <typename Ntk>
  inline void Simulator2<Ntk>::Copy(int n, itr dst, citr src, bool c) const {
    if(!c) {
      for(int i = 0; i < n; i++, dst++, src++) {
        *dst = *src;
      }
    } else {
      for(int i = 0; i < n; i++, dst++, src++) {
        *dst = ~*src;
      }
    }
  }
  
  template <typename Ntk>
  inline void Simulator2<Ntk>::And(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
    if(!c0) {
      if(!c1) {
        for(int i = 0; i < n; i++, dst++, src0++, src1++) {
          *dst = *src0 & *src1;
        }
      } else {
        for(int i = 0; i < n; i++, dst++, src0++, src1++) {
          *dst = *src0 & ~*src1;
        }
      }
    } else {
      if(!c1) {
        for(int i = 0; i < n; i++, dst++, src0++, src1++) {
          *dst = ~*src0 & *src1;
        }
      } else {
        for(int i = 0; i < n; i++, dst++, src0++, src1++) {
          *dst = ~*src0 & ~*src1;
        }
      }
    }
  }

  template <typename Ntk>
  inline void Simulator2<Ntk>::Xor(int n, itr dst, citr src0, citr src1, bool c) const {
    if(!c) {
      for(int i = 0; i < n; i++, dst++, src0++, src1++) {
        *dst = *src0 ^ *src1;
      }
    } else {
      for(int i = 0; i < n; i++, dst++, src0++, src1++) {
        *dst = *src0 ^ ~*src1;
      }
    }
  }

  template <typename Ntk>
  inline bool Simulator2<Ntk>::IsZero(int n, citr x) const {
    for(int i = 0; i < n; i++, x++) {
      if(*x) {
        return false;
      }
    }
    return true;
  }

  template <typename Ntk>
  inline bool Simulator2<Ntk>::IsEq(int n, citr x, citr y) const {
    for(int i = 0; i < n; i++, x++, y++) {
      if(*x != *y) {
        return false;
      }
    }
    return true;
  }

  template <typename Ntk>
  inline void Simulator2<Ntk>::Print(int n, citr x) const {
    std::cout << std::bitset<64>(*x);
    x++;
    for(int i = 1; i < n; i++, x++) {
      std::cout << std::endl << std::string(10, ' ') << std::bitset<64>(*x);
    }
  }
  
  /* }}} */

  /* {{{ Callback */
  
  template <typename Ntk>
  void Simulator2<Ntk>::ActionCallback(Action const &action) {
    switch(action.type) {
    case REMOVE_FANIN:
      assert(fInitialized);
      if(target != -1) {
        if(action.id == target) {
          fUpdate = true;
        } else {
          sUpdates.insert(action.id);
        }
      }
      break;
    case REMOVE_UNUSED:
      break;
    case REMOVE_BUFFER:
    case REMOVE_CONST:
      if(fInitialized) {
        if(target != -1) {
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
      }
      break;
    case ADD_FANIN:
      assert(fInitialized);
      if(target != -1) {
        if(action.id == target) {
          fUpdate = true;
        } else {
          sUpdates.insert(action.id);
        }
      }
      break;
    case TRIVIAL_COLLAPSE:
      break;
    case TRIVIAL_DECOMPOSE:
      if(fInitialized) {
        if(target != -1) {
          vValues.resize(nStimuli * pNtk->GetNumNodes());
          SimulateNode(vValues, action.fi);
          // time of this simulation is not measured for simplicity sake
        }
      }
      break;
    case SORT_FANINS:
      break;
    case READ:
      fInitialized = false;
      break;
    case SAVE:
      break;
    case LOAD:
      fInitialized = false;
      break;
    case POP_BACK:
      break;
    default:
      assert(0);
    }
  }
  
  /* }}} */

  /* {{{ Simulation */
  
  template <typename Ntk>
  void Simulator2<Ntk>::SimulateNode(std::vector<word> &v, int id, int to_negate) {
    itr x = v.end();
    itr y = v.begin() + id * nStimuli;
    bool cx = false;
    switch(pNtk->GetNodeType(id)) {
    case AND:
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nStimuli;
          cx = c ^ (fi == to_negate);
        } else {
          And(nStimuli, y, x, v.begin() + fi * nStimuli, cx, c ^ (fi == to_negate));
          x = y;
          cx = false;
        }
      });
      if(x == v.end()) {
        Fill(nStimuli, y);
      }
      break;
    default:
      assert(0);
    }
  }
      
  template <typename Ntk>
  bool Simulator2<Ntk>::ResimulateNode(std::vector<word> &v, int id, int to_negate) {
    itr x = v.end();
    bool cx = false;
    switch(pNtk->GetNodeType(id)) {
    case AND:
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nStimuli;
          cx = c ^ (fi == to_negate);
        } else {
          And(nStimuli, tmp.begin(), x, v.begin() + fi * nStimuli, cx, c ^ (fi == to_negate));
          x = tmp.begin();
          cx = false;
        }
      });
      if(x == v.end()) {
        Fill(nStimuli, tmp.begin());
      }
      break;
    default:
      assert(0);
    }
    itr y = v.begin() + id * nStimuli;
    if(IsEq(nStimuli, y, tmp.begin())) {
      return false;
    }
    Copy(nStimuli, y, tmp.begin(), false);
    return true;
  }
  
  template <typename Ntk>
  void Simulator2<Ntk>::SimulateOneWordNode(std::vector<word> &v, int id, int offset, int to_negate) {
    itr x = v.end();
    itr y = v.begin() + id * nStimuli + offset;
    bool cx = false;
    switch(pNtk->GetNodeType(id)) {
    case AND:
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nStimuli + offset;
          cx = c ^ (fi == to_negate);
        } else {
          And(1, y, x, v.begin() + fi * nStimuli + offset, cx, c ^ (fi == to_negate));
          x = y;
          cx = false;
        }
      });
      if(x == v.end()) {
        Fill(1, y);
      }
      break;
    default:
      assert(0);
    }
  }
  
  template <typename Ntk>
  void Simulator2<Ntk>::SimulatePartNode(std::vector<word> &v, int id, int n, int offset, int to_negate) {
    itr x = v.end();
    itr y = v.begin() + id * nStimuli + offset;
    bool cx = false;
    switch(pNtk->GetNodeType(id)) {
    case AND:
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nStimuli + offset;
          cx = c ^ (fi == to_negate);
        } else {
          And(n, y, x, v.begin() + fi * nStimuli + offset, cx, c ^ (fi == to_negate));
          x = y;
          cx = false;
        }
      });
      if(x == v.end()) {
        Fill(n, y);
      }
      break;
    default:
      assert(0);
    }
  }
  
  template <typename Ntk>
  void Simulator2<Ntk>::Simulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "simulating" << std::endl;
    }
    pNtk->ForEachInt([&](int id) {
      SimulateNode(vValues, id);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nStimuli, vValues.begin() + id * nStimuli);
        std::cout << std::endl;
      }
    });
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }
  
  template <typename Ntk>
  void Simulator2<Ntk>::Resimulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "resimulating" << std::endl;
    }
    pNtk->ForEachTfosUpdate(sUpdates, false, [&](int fo) {
      bool fUpdated = ResimulateNode(vValues, fo);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << fo << ": ";
        Print(nStimuli, vValues.begin() + fo * nStimuli);
        std::cout << std::endl;
      }
      return fUpdated;
    });
    /* alternative version that updates entire TFO
    pNtk->ForEachTfos(sUpdates, false, [&](int fo) {
      SimulateNode(vValues, fo);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << fo << ": ";
        Print(nStimuli, vValues.begin() + fo * nStimuli);
        std::cout << std::endl;
      }
    });
    */
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }

  template <typename Ntk>
  void Simulator2<Ntk>::SimulateOneWord(int offset) {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "simulating word " << offset << std::endl;
    }
    pNtk->ForEachInt([&](int id) {
      SimulateOneWordNode(vValues, id, offset);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(1, vValues.begin() + id * nStimuli + offset);
        std::cout << std::endl;
      }
    });
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }

  /* }}} */

  /* {{{ Stimuli */

  template <typename Ntk>
  void Simulator2<Ntk>::GenerateRandomStimuli() {
    if(nVerbose) {
      std::cout << "generating random stimuli" << std::endl;
    }
    std::mt19937_64 rng;
    pNtk->ForEachPi([&](int id) {
      for(int i = 0; i < nStimuli; i++) {
        vValues[id * nStimuli + i] = rng();
      }
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nStimuli, vValues.begin() + id * nStimuli);
        std::cout << std::endl;
      }
    });
    Clear(nStimuli * pNtk->GetNumPis(), vAssignedStimuli.begin());
  }

  template <typename Ntk>
  void Simulator2<Ntk>::ReadStimuli(Pattern *pPat) {
    nStimuli = pPat->GetNumWords();
    vValues.resize(nStimuli * pNtk->GetNumNodes());
    pNtk->ForEachPiIdx([&](int index, int id) {
      Copy(nStimuli, vValues.begin() + id * nStimuli, pPat->GetIterator(index), false);
    });
    fGenerated = true;
  }

  /* }}} */

  /* {{{ Careset computation */
  
  template <typename Ntk>
  void Simulator2<Ntk>::ComputeCare(int id) {
    if(sUpdates.empty() && id == target) {
      return;
    }
    if(fUpdate) {
      sUpdates.insert(target);
      fUpdate = false;
    }
    if(!sUpdates.empty()) {
      Resimulate();
      sUpdates.clear();
    }
    target = id;
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "computing careset of " << target << std::endl;
    }
    if(pNtk->IsPoDriver(target)) {
      Fill(nStimuli, care.begin());
      if(nVerbose) {
        std::cout << "care " << std::setw(3) << target << ": ";
        Print(nStimuli, care.begin());
        std::cout << std::endl;
      }
      durationCare += Duration(timeStart, GetCurrentTime());
      return;
    }
    vValues2 = vValues;
    pNtk->ForEachTfo(target, false, [&](int id) {
      SimulateNode(vValues2, id, target);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nStimuli, vValues2.begin() + id * nStimuli);
        std::cout << std::endl;
      }
    });
    /* alternative version that updates only affected TFO
    pNtk->ForEachTfoUpdate(target, false, [&](int id) {
      bool fUpdated = ResimulateNode(vValues2, id, target);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nStimuli, vValues2.begin() + id * nStimuli);
        std::cout << std::endl;
      }
      return fUpdated;
    });
    */
    Clear(nStimuli, care.begin());
    pNtk->ForEachPoDriver([&](int fi) {
      assert(fi != target);
      for(int i = 0; i < nStimuli; i++) {
        care[i] = care[i] | (vValues[fi * nStimuli + i] ^ vValues2[fi * nStimuli + i]);
      }
    });
    if(nVerbose) {
      std::cout << "care " << std::setw(3) << target << ": ";
      Print(nStimuli, care.begin());
      std::cout << std::endl;
    }
    durationCare += Duration(timeStart, GetCurrentTime());
  }

  template <typename Ntk>
  void Simulator2<Ntk>::ComputeCarePart(int nWords_, int offset) {
    if(nVerbose) {
      std::cout << "computing careset of " << target << " offset " << offset << std::endl;
    }
    if(pNtk->IsPoDriver(target)) {
      Fill(nWords_, care.begin());
      if(nVerbose) {
        std::cout << "care " << std::setw(3) << target << ": ";
        Print(nWords_, care.begin());
        std::cout << std::endl;
      }
      return;
    }
    pNtk->ForEachTfo(target, false, [&](int id) {
      SimulatePartNode(vValues2, id, nWords_, offset, target);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nWords_, vValues2.begin() + id * nStimuli + offset);
        std::cout << std::endl;
      }
    });
    Clear(nWords_, care.begin());
    pNtk->ForEachPoDriver([&](int fi) {
      assert(fi != target);
      for(int i = 0; i < nWords_; i++) {
        care[i] = care[i] | (vValues[fi * nStimuli + i + offset] ^ vValues2[fi * nStimuli + i + offset]);
      }
    });
    if(nVerbose) {
      std::cout << "care " << std::setw(3) << target << ": ";
      Print(nWords_, care.begin());
      std::cout << std::endl;
    }
  }

  /* }}} */

  /* {{{ Preparation */

  template <typename Ntk>
  void Simulator2<Ntk>::Initialize() {
    if(!fGenerated) {
      // TODO: reset nStimuli to default here maybe, if such a mechanism that changes nStimuli has been implemneted
      vValues.resize(nStimuli * pNtk->GetNumNodes());
      iPivot = 0;
      vAssignedStimuli.clear();
      vAssignedStimuli.resize(nStimuli * pNtk->GetNumPis());
      for(int count: vPackedCount) {
        if(count) {
          vPackedCountEvicted.push_back(count);
        }
      }
      vPackedCount.clear();
      vPackedCount.resize(nStimuli * 64);
      GenerateRandomStimuli();
      fGenerated = true;
    } else {
      // use same nStimuli as we are reusing patterns even if nStimuli has changed
      vValues.resize(nStimuli * pNtk->GetNumNodes());
    }
    Simulate();
    fInitialized = true;
  }

  /* }}} */
  
  /* {{{ Constructors */
  
  template <typename Ntk>
  Simulator2<Ntk>::Simulator2() :
    pNtk(NULL),
    nVerbose(0),
    nWords(0),
    nStimuli(0),
    fGenerated(false),
    fInitialized(false),
    target(-1),
    iPivot(0),
    fUpdate(false) {
    ResetSummary();
  }
  
  template <typename Ntk>
  Simulator2<Ntk>::Simulator2(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nSimulatorVerbose),
    nWords(pPar->nWords),
    nStimuli(nWords),
    fGenerated(false),
    fInitialized(false),
    target(-1),
    iPivot(0),
    fUpdate(false) {
    care.resize(nWords);
    tmp.resize(nWords);
    ResetSummary();
  }

  template <typename Ntk>
  void Simulator2<Ntk>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    if(!fReuse) {
      fGenerated = false;
    }
    fInitialized = false;
    target = -1;
    fUpdate = false;
    sUpdates.clear();
    pNtk = pNtk_;
    pNtk->AddCallback(std::bind(&Simulator2<Ntk>::ActionCallback, this, std::placeholders::_1));
    Pattern *pPat = pNtk->GetPattern();
    if(pPat) {
      ReadStimuli(pPat);
      care.resize(nStimuli);
      tmp.resize(nStimuli);
    }
  }

  /* }}} */

  /* {{{ Checks */
  
  template <typename Ntk>
  bool Simulator2<Ntk>::CheckRedundancy(int id, int idx) {
    if(!fInitialized) {
      Initialize();
    }
    ComputeCare(id);
    switch(pNtk->GetNodeType(id)) {
    case AND: {
      itr x = vValues.end();
      bool cx = false;
      pNtk->ForEachFaninIdx(id, [&](int idx2, int fi, bool c) {
        if(idx == idx2) {
          return;
        }
        if(x == vValues.end()) {
          x = vValues.begin() + fi * nStimuli;
          cx = c;
        } else {
          And(nStimuli, tmp.begin(), x, vValues.begin() + fi * nStimuli, cx, c);
          x = tmp.begin();
          cx = false;
        }
      });
      if(x == vValues.end()) {
        x = care.begin();
      } else {
        And(nStimuli, tmp.begin(), x, care.begin(), cx, false);
        x = tmp.begin();
      }
      int fi = pNtk->GetFanin(id, idx);
      bool c = pNtk->GetCompl(id, idx);
      And(nStimuli, tmp.begin(), x, vValues.begin() + fi * nStimuli, false, !c);
      return IsZero(nStimuli, tmp.begin());
    }
    default:
      assert(0);
    }
    return false;
    /* simulate for every nWords 
    assert(nStimuli > nWords);
    // prepare sim
    if(fUpdate) {
      sUpdates.insert(target);
      fUpdate = false;
    }
    if(!sUpdates.empty()) {
      Resimulate();
      sUpdates.clear();
    }
    target = id;
    // check for every nWords
    time_point timeStart = GetCurrentTime();
    vValues2 = vValues;
    int offset = 0;
    bool fRedundant = true;
    while(fRedundant && offset < nStimuli) {
      int nWords_ = nWords;
      if(offset + nWords > nStimuli) {
        nWords_ = nStimuli - offset;
      }
      ComputeCarePart(nWords_, offset);
      // check
      switch(pNtk->GetNodeType(id)) {
      case AND: {
        itr x = vValues.end();
        bool cx = false;
        pNtk->ForEachFaninIdx(id, [&](int idx2, int fi, bool c) {
          if(idx == idx2) {
            return;
          }
          if(x == vValues.end()) {
            x = vValues.begin() + fi * nStimuli + offset;
            cx = c;
          } else {
            And(nWords_, tmp.begin(), x, vValues.begin() + fi * nStimuli + offset, cx, c);
            x = tmp.begin();
            cx = false;
          }
        });
        if(x == vValues.end()) {
          x = care.begin();
        } else {
          And(nWords_, tmp.begin(), x, care.begin(), cx, false);
          x = tmp.begin();
        }
        int fi = pNtk->GetFanin(id, idx);
        bool c = pNtk->GetCompl(id, idx);
        And(nWords_, tmp.begin(), x, vValues.begin() + fi * nStimuli + offset, false, !c);
        fRedundant &= IsZero(nWords_, tmp.begin());
        break;
      }
      default:
        assert(0);
      }
      offset += nWords;
    }
    durationCare += Duration(timeStart, GetCurrentTime());
    return fRedundant;
    */
  }

  template <typename Ntk>
  bool Simulator2<Ntk>::CheckFeasibility(int id, int fi, bool c) {
    if(!fInitialized) {
      Initialize();
    }
    ComputeCare(id);
    switch(pNtk->GetNodeType(id)) {
    case AND: {
      itr x = vValues.end();
      bool cx = false;
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        if(x == vValues.end()) {
          x = vValues.begin() + fi * nStimuli;
          cx = c;
        } else {
          And(nStimuli, tmp.begin(), x, vValues.begin() + fi * nStimuli, cx, c);
          x = tmp.begin();
          cx = false;
        }
      });
      if(x == vValues.end()) {
        x = care.begin();
      } else {
        And(nStimuli, tmp.begin(), x, care.begin(), cx, false);
        x = tmp.begin();
      }
      And(nStimuli, tmp.begin(), x, vValues.begin() + fi * nStimuli, false, !c);
      return IsZero(nStimuli, tmp.begin());
    }
    default:
      assert(0);
    }
    return false;
  }

  /* }}} */

  /* {{{ Cex */

  template <typename Ntk>
  void Simulator2<Ntk>::AddCex(std::vector<VarValue> const &vCex) {
    if(nVerbose) {
      std::cout << "cex: ";
      for(VarValue c: vCex) {
        std::cout << GetVarValueChar(c);
      }
      std::cout << std::endl;
    }
    // record care pi indices
    assert(int_size(vCex) == pNtk->GetNumPis());
    std::vector<int> vCarePiIdxs;
    for(int idx = 0; idx < pNtk->GetNumPis(); idx++) {
      switch(vCex[idx]) {
      case TRUE:
        vCarePiIdxs.push_back(idx);
        break;
      case FALSE:
        vCarePiIdxs.push_back(idx);
        break;
      default:
        break;
      }
    }
    assert(!vCarePiIdxs.empty());
    // find compatible word
    int iWord = 0;
    std::vector<word> vCompatibleBits(1);
    itr it = vCompatibleBits.begin();
    for(; iWord < nStimuli; iWord++) {
      Fill(1, it);
      for(int idx: vCarePiIdxs) {
        int id = pNtk->GetPi(idx);
        bool c;
        if(vCex[idx] == TRUE) {
          c = false;
        } else {
          assert(vCex[idx] == FALSE);
          c = true;
        }
        itr x = vValues.begin() + id * nStimuli + iWord;
        itr y = vAssignedStimuli.begin() + idx * nStimuli + iWord;
        And(1, tmp.begin(), x, y, !c, false);
        And(1, it, it, tmp.begin(), false, true);
        if(IsZero(1, it)) {
          break;
        }
      }
      if(!IsZero(1, it)) {
        break;
      }
    }
    // find compatible bit
    int iBit;
    if(iWord < nStimuli) {
      assert(!IsZero(1, it));
      iBit = 0;
      while(!((*it >> iBit) & 1)) {
        iBit++;
      }
      if(nVerbose) {
        std::cout << "fusing into stimulus word " << iWord << " bit " << iBit << std::endl;
      }
      vPackedCount[iWord * 64 + iBit]++;
    } else {
      // no bits are compatible, so reset at pivot
      iWord = iPivot / 64;
      iBit = iPivot % 64;
      if(nVerbose) {
        std::cout << "resetting stimulus word " << iWord << " bit " << iBit << std::endl;
      }
      if(vPackedCount[iWord * 64 + iBit]) {
        // this can be zero only when stats has been reset
        vPackedCountEvicted.push_back(vPackedCount[iWord * 64 + iBit]);
      }
      vPackedCount[iWord * 64 + iBit] = 1;
      word mask = 1ull << iBit;
      for(int idx = 0; idx < pNtk->GetNumPis(); idx++) {
        vAssignedStimuli[idx * nStimuli + iWord] &= ~mask;
      }
      iPivot++;
      if(iPivot == 64 * nStimuli) {
        iPivot = 0;
      }
    }
    // update stimulus
    for(int idx: vCarePiIdxs) {
      int id = pNtk->GetPi(idx);
      word mask = 1ull << iBit;
      if(vCex[idx] == TRUE) {
        vValues[id * nStimuli + iWord] |= mask;
      } else {
        assert(vCex[idx] == FALSE);
        vValues[id * nStimuli + iWord] &= ~mask;
      }
      vAssignedStimuli[idx * nStimuli + iWord] |= mask;
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(1, vValues.begin() + id * nStimuli + iWord);
        std::cout << std::endl;
        std::cout << "asgn " << std::setw(3) << id << ": ";
        Print(1, vAssignedStimuli.begin() + idx * nStimuli + iWord);
        std::cout << std::endl;
      }
    }
    // simulate
    SimulateOneWord(iWord);
    // recompute care with new stimulus
    time_point timeStart = GetCurrentTime();
    if(target != -1 && !pNtk->IsPoDriver(target)) {
      if(nVerbose) {
        std::cout << "recomputing careset of " << target << std::endl;
      }
      vValues2.resize(vValues.size());
      pNtk->ForEachPi([&](int id) {
        vValues2[id * nStimuli + iWord] = vValues[id * nStimuli + iWord];
      });
      pNtk->ForEachInt([&](int id) {
        vValues2[id * nStimuli + iWord] = vValues[id * nStimuli + iWord];
      });
      pNtk->ForEachTfo(target, false, [&](int id) {
        SimulateOneWordNode(vValues2, id, iWord, target);
        if(nVerbose) {
          std::cout << "node " << std::setw(3) << id << ": ";
          Print(1, vValues2.begin() + id * nStimuli + iWord);
          std::cout << std::endl;
        }
      });
      Clear(1, care.begin() + iWord);
      pNtk->ForEachPoDriver([&](int fi) {
        assert(fi != target);
        care[iWord] = care[iWord] | (vValues[fi * nStimuli + iWord] ^ vValues2[fi * nStimuli + iWord]);
      });
      if(nVerbose) {
        std::cout << "care " << std::setw(3) << target << ": ";
        Print(1, care.begin() + iWord);
        std::cout << std::endl;
      }
    }
    durationCare += Duration(timeStart, GetCurrentTime());
    nCex++;
  }
  
  /* }}} */

  /* {{{ Summary */
  
  template <typename Ntk>
  void Simulator2<Ntk>::ResetSummary() {
    nCex = 0;
    nDiscarded = 0;
    nPackedCountOld = 0;
    for(int count: vPackedCount) {
      if(count) {
        nPackedCountOld++;
      }
    }
    vPackedCountEvicted.clear();
    durationSimulation = 0;
    durationCare = 0;
  };
  
  template <typename Ntk>
  summary<int> Simulator2<Ntk>::GetStatsSummary() const {
    summary<int> v;
    v.emplace_back("sim cex", nCex);
    if(!fKeepStimuli) {
      v.emplace_back("sim discarded cex", nDiscarded);
    }
    int nPackedCount = vPackedCountEvicted.size() - nPackedCountOld;
    for(int count: vPackedCount) {
      if(count) {
        nPackedCount++;
      }
    }
    v.emplace_back("sim packed pattern", nPackedCount);
    v.emplace_back("sim evicted pattern", vPackedCountEvicted.size());
    return v;
  };
  
  template <typename Ntk>
  summary<double> Simulator2<Ntk>::GetTimesSummary() const {
    summary<double> v;
    v.emplace_back("sim simulation", durationSimulation);
    v.emplace_back("sim care computation", durationCare);
    return v;
  };
  
  /* }}} */
  
}
