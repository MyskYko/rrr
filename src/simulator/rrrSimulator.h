#pragma once

#include <algorithm>
#include <random>
#include <bitset>

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"

namespace rrr {

  template <typename Ntk>
  class Simulator {
  private:
    // aliases
    using word = unsigned long long;
    using itr = std::vector<word>::iterator;
    using citr = std::vector<word>::const_iterator;
    static constexpr word one = 0xffffffffffffffff;
    static constexpr bool fKeepStimula = true;

    // pointer to network
    Ntk *pNtk;
    
    // parameters
    int nVerbose;
    int nWords;

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

    // backups
    std::vector<Simulator> vBackups;

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
    void Simulate();
    void Resimulate();
    void SimulateOneWord(int offset);

    // generate stimuli
    void GenerateRandomStimuli();

    // careset computation
    void ComputeCare(int id);

    // preparation
    void Initialize();

    // save & load
    void Save(int slot);
    void Load(int slot);

  public:
    // constructors
    Simulator();
    Simulator(Parameter const *pPar);
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
  inline void Simulator<Ntk>::Clear(int n, itr x) const {
    std::fill(x, x + n, 0);
  }

  template <typename Ntk>
  inline void Simulator<Ntk>::Fill(int n, itr x) const {
    std::fill(x, x + n, one);
  }
  
  template <typename Ntk>
  inline void Simulator<Ntk>::Copy(int n, itr dst, citr src, bool c) const {
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
  inline void Simulator<Ntk>::And(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
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
  inline void Simulator<Ntk>::Xor(int n, itr dst, citr src0, citr src1, bool c) const {
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
  inline bool Simulator<Ntk>::IsZero(int n, citr x) const {
    for(int i = 0; i < n; i++, x++) {
      if(*x) {
        return false;
      }
    }
    return true;
  }

  template <typename Ntk>
  inline bool Simulator<Ntk>::IsEq(int n, citr x, citr y) const {
    for(int i = 0; i < n; i++, x++, y++) {
      if(*x != *y) {
        return false;
      }
    }
    return true;
  }

  template <typename Ntk>
  inline void Simulator<Ntk>::Print(int n, citr x) const {
    std::cout << std::bitset<64>(*x);
    x++;
    for(int i = 1; i < n; i++, x++) {
      std::cout << std::endl << std::string(10, ' ') << std::bitset<64>(*x);
    }
  }
  
  /* }}} */

  /* {{{ Callback */
  
  template <typename Ntk>
  void Simulator<Ntk>::ActionCallback(Action const &action) {
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
          vValues.resize(nWords * pNtk->GetNumNodes());
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
      Save(action.idx);
      break;
    case LOAD:
      Load(action.idx);
      break;
    case POP_BACK:
      // Do nothing: it may be good to keep the word vector allocated
      break;
    default:
      assert(0);
    }
  }
  
  /* }}} */

  /* {{{ Simulation */
  
  template <typename Ntk>
  void Simulator<Ntk>::SimulateNode(std::vector<word> &v, int id, int to_negate) {
    itr x = v.end();
    itr y = v.begin() + id * nWords;
    bool cx = false;
    switch(pNtk->GetNodeType(id)) {
    case AND:
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nWords;
          cx = c ^ (fi == to_negate);
        } else {
          And(nWords, y, x, v.begin() + fi * nWords, cx, c ^ (fi == to_negate));
          x = y;
          cx = false;
        }
      });
      if(x == v.end()) {
        Fill(nWords, y);
      } else if(x != y) {
        Copy(nWords, y, x, cx);
      }
      break;
    default:
      assert(0);
    }
  }
      
  template <typename Ntk>
  bool Simulator<Ntk>::ResimulateNode(std::vector<word> &v, int id, int to_negate) {
    itr x = v.end();
    bool cx = false;
    switch(pNtk->GetNodeType(id)) {
    case AND:
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nWords;
          cx = c ^ (fi == to_negate);
        } else {
          And(nWords, tmp.begin(), x, v.begin() + fi * nWords, cx, c ^ (fi == to_negate));
          x = tmp.begin();
          cx = false;
        }
      });
      if(x == v.end()) {
        Fill(nWords, tmp.begin());
      } else if(x != tmp.begin()) {
        Copy(nWords, tmp.begin(), x, cx);
      }
      break;
    default:
      assert(0);
    }
    itr y = v.begin() + id * nWords;
    if(IsEq(nWords, y, tmp.begin())) {
      return false;
    }
    Copy(nWords, y, tmp.begin(), false);
    return true;
  }
  
  template <typename Ntk>
  void Simulator<Ntk>::SimulateOneWordNode(std::vector<word> &v, int id, int offset, int to_negate) {
    itr x = v.end();
    itr y = v.begin() + id * nWords + offset;
    bool cx = false;
    switch(pNtk->GetNodeType(id)) {
    case AND:
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nWords + offset;
          cx = c ^ (fi == to_negate);
        } else {
          And(1, y, x, v.begin() + fi * nWords + offset, cx, c ^ (fi == to_negate));
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
  void Simulator<Ntk>::Simulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "simulating" << std::endl;
    }
    pNtk->ForEachInt([&](int id) {
      SimulateNode(vValues, id);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nWords, vValues.begin() + id * nWords);
        std::cout << std::endl;
      }
    });
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }
  
  template <typename Ntk>
  void Simulator<Ntk>::Resimulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "resimulating" << std::endl;
    }
    pNtk->ForEachTfosUpdate(sUpdates, false, [&](int fo) {
      bool fUpdated = ResimulateNode(vValues, fo);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << fo << ": ";
        Print(nWords, vValues.begin() + fo * nWords);
        std::cout << std::endl;
      }
      return fUpdated;
    });
    /* alternative version that updates entire TFO
    pNtk->ForEachTfos(sUpdates, false, [&](int fo) {
      SimulateNode(vValues, fo);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << fo << ": ";
        Print(nWords, vValues.begin() + fo * nWords);
        std::cout << std::endl;
      }
    });
    */
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }

  template <typename Ntk>
  void Simulator<Ntk>::SimulateOneWord(int offset) {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "simulating word " << offset << std::endl;
    }
    pNtk->ForEachInt([&](int id) {
      SimulateOneWordNode(vValues, id, offset);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(1, vValues.begin() + id * nWords + offset);
        std::cout << std::endl;
      }
    });
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }

  /* }}} */

  /* {{{ Generate stimuli */

  template <typename Ntk>
  void Simulator<Ntk>::GenerateRandomStimuli() {
    if(nVerbose) {
      std::cout << "generating random stimuli" << std::endl;
    }
    std::mt19937_64 rng;
    pNtk->ForEachPi([&](int id) {
      for(int i = 0; i < nWords; i++) {
        vValues[id * nWords + i] = rng();
      }
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nWords, vValues.begin() + id * nWords);
        std::cout << std::endl;
      }
    });
    Clear(nWords * pNtk->GetNumPis(), vAssignedStimuli.begin());
  }

  /* }}} */

  /* {{{ Careset computation */
  
  template <typename Ntk>
  void Simulator<Ntk>::ComputeCare(int id) {
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
      Fill(nWords, care.begin());
      if(nVerbose) {
        std::cout << "care " << std::setw(3) << target << ": ";
        Print(nWords, care.begin());
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
        Print(nWords, vValues2.begin() + id * nWords);
        std::cout << std::endl;
      }
    });
    /* alternative version that updates only affected TFO
    pNtk->ForEachTfoUpdate(target, false, [&](int id) {
      bool fUpdated = ResimulateNode(vValues2, id, target);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nWords, vValues2.begin() + id * nWords);
        std::cout << std::endl;
      }
      return fUpdated;
    });
    */
    Clear(nWords, care.begin());
    pNtk->ForEachPoDriver([&](int fi) {
      assert(fi != target);
      for(int i = 0; i < nWords; i++) {
        care[i] = care[i] | (vValues[fi * nWords + i] ^ vValues2[fi * nWords + i]);
      }
    });
    if(nVerbose) {
      std::cout << "care " << std::setw(3) << target << ": ";
      Print(nWords, care.begin());
      std::cout << std::endl;
    }
    durationCare += Duration(timeStart, GetCurrentTime());
  }
  
  /* }}} */

  /* {{{ Preparation */

  template <typename Ntk>
  void Simulator<Ntk>::Initialize() {
    if(!fGenerated) {
      // TODO: reset nWords to default here maybe, if such a mechanism that changes nWords has been implemneted
      vValues.resize(nWords * pNtk->GetNumNodes());
      iPivot = 0;
      vAssignedStimuli.clear();
      vAssignedStimuli.resize(nWords * pNtk->GetNumPis());
      for(int count: vPackedCount) {
        if(count) {
          vPackedCountEvicted.push_back(count);
        }
      }
      vPackedCount.clear();
      vPackedCount.resize(nWords * 64);
      GenerateRandomStimuli();
      fGenerated = true;
    } else {
      // use same nWords as we are reusing patterns even if nWords has changed
      vValues.resize(nWords * pNtk->GetNumNodes());
    }
    Simulate();
    fInitialized = true;
  }

  /* }}} */

  /* {{{ Save & load */

  template <typename Ntk>
  void Simulator<Ntk>::Save(int slot) {
    assert(slot >= 0);
    assert(!check_int_max(slot));
    if(slot >= int_size(vBackups)) {
      vBackups.resize(slot + 1);
    }
    vBackups[slot].fInitialized = fInitialized;
    if(!fInitialized) {
      return;
    }
    vBackups[slot].nWords = nWords;
    if(sUpdates.empty()) {
      vBackups[slot].target = target;
      vBackups[slot].care = care;
    } else {
      vBackups[slot].target = -1;
      vBackups[slot].care = care;
    }
    if(fUpdate) {
      sUpdates.insert(target);
      fUpdate = false;
    }
    if(!sUpdates.empty()) {
      Resimulate();
      sUpdates.clear();
    }
    vBackups[slot].vValues = vValues;
    vBackups[slot].iPivot = iPivot;
    vBackups[slot].vAssignedStimuli = vAssignedStimuli;
    target = vBackups[slot].target; // assigned to -1 when careset needs updating
    if(!fKeepStimula) {
      vBackups[slot].nCex = nCex;
      vBackups[slot].nPackedCountOld = nPackedCountOld;
      vBackups[slot].vPackedCount = vPackedCount;
      vBackups[slot].vPackedCountEvicted = vPackedCountEvicted;
    }
  }

  template <typename Ntk>
  void Simulator<Ntk>::Load(int slot) {
    assert(slot >= 0);
    assert(slot < int_size(vBackups));
    fUpdate = false;
    sUpdates.clear();
    if(!vBackups[slot].fInitialized) {
      fInitialized = false;
      return;
    }
    if(!fKeepStimula) {
      nWords  = vBackups[slot].nWords;
      target  = vBackups[slot].target;
      vValues = vBackups[slot].vValues;
      care    = vBackups[slot].care;
      iPivot  = vBackups[slot].iPivot;
      vAssignedStimuli = vBackups[slot].vAssignedStimuli;
      nDiscarded += nCex - vBackups[slot].nCex;
      nCex = vBackups[slot].nCex;
      nPackedCountOld = vBackups[slot].nPackedCountOld;
      vPackedCount = vBackups[slot].vPackedCount;
      vPackedCountEvicted = vBackups[slot].vPackedCountEvicted;
      tmp.resize(nWords);
    } else {
      std::vector<int> vOffsets;
      for(int i = 0; i < vBackups[slot].nWords; i++) {
        bool fDifferent = false;
        pNtk->ForEachPi([&](int id) {
          if(vValues[id * vBackups[slot].nWords + i] != vValues[id * nWords + i]) {
            fDifferent = true;
          }
        });
        if(fDifferent) {
          vOffsets.push_back(i);
        }
      }
      if(nWords == vBackups[slot].nWords) {
        if(vOffsets.empty()) {
          target  = vBackups[slot].target;
          vValues = vBackups[slot].vValues;
          care    = vBackups[slot].care;
        } else {
          target = -1;
          std::vector<std::vector<word>> vInputStimuli(pNtk->GetNumPis());
          pNtk->ForEachPiIdx([&](int idx, int id) {
            vInputStimuli[idx].resize(nWords);
            Copy(nWords, vInputStimuli[idx].begin(), vValues.begin() + id * nWords, false);
          });
          vValues = vBackups[slot].vValues;
          pNtk->ForEachPiIdx([&](int idx, int id) {
            Copy(nWords, vValues.begin() + id * nWords, vInputStimuli[idx].begin(), false);
          });
          for(int i: vOffsets) {
            SimulateOneWord(i);
          }
        }
      } else {
        // TODO: when nWords has changed
        assert(0);
      }
    }
  }
  
  /* }}} */
  
  /* {{{ Constructors */
  
  template <typename Ntk>
  Simulator<Ntk>::Simulator() :
    pNtk(NULL),
    nVerbose(0),
    nWords(0),
    fGenerated(false),
    fInitialized(false),
    target(-1),
    iPivot(0),
    fUpdate(false) {
    ResetSummary();
  }
  
  template <typename Ntk>
  Simulator<Ntk>::Simulator(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nSimulatorVerbose),
    nWords(pPar->nWords),
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
  void Simulator<Ntk>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    if(!fReuse) {
      fGenerated = false;
    }
    fInitialized = false;
    target = -1;
    fUpdate = false;
    sUpdates.clear();
    pNtk = pNtk_;
    pNtk->AddCallback(std::bind(&Simulator<Ntk>::ActionCallback, this, std::placeholders::_1));
  }

  /* }}} */

  /* {{{ Checks */
  
  template <typename Ntk>
  bool Simulator<Ntk>::CheckRedundancy(int id, int idx) {
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
          x = vValues.begin() + fi * nWords;
          cx = c;
        } else {
          And(nWords, tmp.begin(), x, vValues.begin() + fi * nWords, cx, c);
          x = tmp.begin();
          cx = false;
        }
      });
      if(x == vValues.end()) {
        x = care.begin();
      } else {
        And(nWords, tmp.begin(), x, care.begin(), cx, false);
        x = tmp.begin();
      }
      int fi = pNtk->GetFanin(id, idx);
      bool c = pNtk->GetCompl(id, idx);
      And(nWords, tmp.begin(), x, vValues.begin() + fi * nWords, false, !c);
      return IsZero(nWords, tmp.begin());
    }
    default:
      assert(0);
    }
    return false;
  }

  template <typename Ntk>
  bool Simulator<Ntk>::CheckFeasibility(int id, int fi, bool c) {
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
          x = vValues.begin() + fi * nWords;
          cx = c;
        } else {
          And(nWords, tmp.begin(), x, vValues.begin() + fi * nWords, cx, c);
          x = tmp.begin();
          cx = false;
        }
      });
      if(x == vValues.end()) {
        x = care.begin();
      } else {
        And(nWords, tmp.begin(), x, care.begin(), cx, false);
        x = tmp.begin();
      }
      And(nWords, tmp.begin(), x, vValues.begin() + fi * nWords, false, !c);
      return IsZero(nWords, tmp.begin());
    }
    default:
      assert(0);
    }
    return false;
  }

  /* }}} */

  /* {{{ Cex */

  template <typename Ntk>
  void Simulator<Ntk>::AddCex(std::vector<VarValue> const &vCex) {
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
      case rrrTRUE:
        vCarePiIdxs.push_back(idx);
        break;
      case rrrFALSE:
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
    for(; iWord < nWords; iWord++) {
      Fill(1, it);
      for(int idx: vCarePiIdxs) {
        int id = pNtk->GetPi(idx);
        bool c;
        if(vCex[idx] == rrrTRUE) {
          c = false;
        } else {
          assert(vCex[idx] == rrrFALSE);
          c = true;
        }
        itr x = vValues.begin() + id * nWords + iWord;
        itr y = vAssignedStimuli.begin() + idx * nWords + iWord;
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
    if(iWord < nWords) {
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
        vAssignedStimuli[idx * nWords + iWord] &= ~mask;
      }
      iPivot++;
      if(iPivot == 64 * nWords) {
        iPivot = 0;
      }
    }
    // update stimulus
    for(int idx: vCarePiIdxs) {
      int id = pNtk->GetPi(idx);
      word mask = 1ull << iBit;
      if(vCex[idx] == rrrTRUE) {
        vValues[id * nWords + iWord] |= mask;
      } else {
        assert(vCex[idx] == rrrFALSE);
        vValues[id * nWords + iWord] &= ~mask;
      }
      vAssignedStimuli[idx * nWords + iWord] |= mask;
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(1, vValues.begin() + id * nWords + iWord);
        std::cout << std::endl;
        std::cout << "asgn " << std::setw(3) << id << ": ";
        Print(1, vAssignedStimuli.begin() + idx * nWords + iWord);
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
        vValues2[id * nWords + iWord] = vValues[id * nWords + iWord];
      });
      pNtk->ForEachInt([&](int id) {
        vValues2[id * nWords + iWord] = vValues[id * nWords + iWord];
      });
      pNtk->ForEachTfo(target, false, [&](int id) {
        SimulateOneWordNode(vValues2, id, iWord, target);
        if(nVerbose) {
          std::cout << "node " << std::setw(3) << id << ": ";
          Print(1, vValues2.begin() + id * nWords + iWord);
          std::cout << std::endl;
        }
      });
      Clear(1, care.begin() + iWord);
      pNtk->ForEachPoDriver([&](int fi) {
        assert(fi != target);
        care[iWord] = care[iWord] | (vValues[fi * nWords + iWord] ^ vValues2[fi * nWords + iWord]);
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
  void Simulator<Ntk>::ResetSummary() {
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
  summary<int> Simulator<Ntk>::GetStatsSummary() const {
    summary<int> v;
    v.emplace_back("sim cex", nCex);
    if(!fKeepStimula) {
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
  summary<double> Simulator<Ntk>::GetTimesSummary() const {
    summary<double> v;
    v.emplace_back("sim simulation", durationSimulation);
    v.emplace_back("sim care computation", durationCare);
    return v;
  };
  
  /* }}} */
  
}
