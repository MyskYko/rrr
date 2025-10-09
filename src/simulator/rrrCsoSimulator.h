#pragma once

#include <algorithm>
#include <random>
#include <bitset>

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"
#include "extra/rrrPattern.h"

namespace rrr {

  template <typename Ntk>
  class CsoSimulator {
  private:
    // aliases
    using word = unsigned long long;
    using itr = std::vector<word>::iterator;
    using citr = std::vector<word>::const_iterator;
    static constexpr word one = 0xffffffffffffffff;
    static constexpr word basepats[] = {0xaaaaaaaaaaaaaaaaull,
                                        0xccccccccccccccccull,
                                        0xf0f0f0f0f0f0f0f0ull,
                                        0xff00ff00ff00ff00ull,
                                        0xffff0000ffff0000ull,
                                        0xffffffff00000000ull};
    // pointer to network
    Ntk *pNtk;
    
    // parameters
    int nVerbose;
    bool fSave;

    // data
    bool fGenerated;
    bool fInitialized;
    int nWords;
    word wLastMask;
    int nMaskedWords;
    int target; // node for which the careset has been computed
    std::vector<word> vValues;
    std::vector<word> vValues2; // simulation with an inverter
    std::vector<word> care; // careset
    std::vector<word> tmp;
    
    // backups
    std::vector<CsoSimulator> vBackups;

    // marks
    unsigned iTrav;
    std::vector<unsigned> vTrav;
    std::vector<unsigned> vTravCond;

    // updates
    bool fUpdate;
    std::set<int> sUpdates;

    // stats
    double durationSimulation;
    double durationCare;
    
    // vector computations
    void Clear(int n, itr x) const;
    void Fill(int n, itr x) const;
    void Copy(int n, itr dst, citr src, bool c) const;
    void And(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const;
    void Or(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const;
    void Xor(int n, itr dst, citr src0, citr src1, bool c) const;
    bool IsZero(int n, citr x, word mask) const;
    int  FindOne(int n, citr x, word mask) const;
    int  FindOneBit(word x) const;
    bool IsEq(int n, citr x, citr y, bool c, word mask) const;
    void Print(int n, citr x) const;

    // callback
    void ActionCallback(Action const &action);

    // topology
    unsigned StartTraversal(int n = 1);
    
    // simulation
    void SimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) const;
    bool ResimulateNode(Ntk *pNtk_, std::vector<word> &v, int id);
    void Simulate();
    void Resimulate();

    // generate stimuli
    void ReadStimuli(Pattern *pPat);

    // careset computation
    void ComputeCare(int id);

    // preparation
    void Initialize();

    // save & load
    void Save(int slot);
    void Load(int slot);
    
  public:
    // constructors
    CsoSimulator();
    CsoSimulator(Parameter const *pPar);
    void AssignNetwork(Ntk *pNtk_, bool fReuse);

    // checks
    bool CheckRedundancy(int id, int idx);
    bool CheckFeasibility(int id, int fi, bool c);

    // others
    void DropLastPattern();

    // summary
    void ResetSummary();
    summary<int> GetStatsSummary() const;
    summary<double> GetTimesSummary() const;
  };


  /* {{{ Vector computations */
  
  template <typename Ntk>
  inline void CsoSimulator<Ntk>::Clear(int n, itr x) const {
    std::fill(x, x + n, 0);
  }

  template <typename Ntk>
  inline void CsoSimulator<Ntk>::Fill(int n, itr x) const {
    std::fill(x, x + n, one);
  }
  
  template <typename Ntk>
  inline void CsoSimulator<Ntk>::Copy(int n, itr dst, citr src, bool c) const {
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
  inline void CsoSimulator<Ntk>::And(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
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
  inline void CsoSimulator<Ntk>::Or(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
    if(!c0) {
      if(!c1) {
        for(int i = 0; i < n; i++, dst++, src0++, src1++) {
          *dst = *src0 | *src1;
        }
      } else {
        for(int i = 0; i < n; i++, dst++, src0++, src1++) {
          *dst = *src0 | ~*src1;
        }
      }
    } else {
      if(!c1) {
        for(int i = 0; i < n; i++, dst++, src0++, src1++) {
          *dst = ~*src0 | *src1;
        }
      } else {
        for(int i = 0; i < n; i++, dst++, src0++, src1++) {
          *dst = ~*src0 | ~*src1;
        }
      }
    }
  }

  template <typename Ntk>
  inline void CsoSimulator<Ntk>::Xor(int n, itr dst, citr src0, citr src1, bool c) const {
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
  inline bool CsoSimulator<Ntk>::IsZero(int n, citr x, word mask) const {
    if(mask == one) {
      for(int i = 0; i < n; i++, x++) {
        if(*x) {
          return false;
        }
      }
    } else {
      for(int i = 0; i < n - 1; i++, x++) {
        if(*x) {
          return false;
        }
      }
      if(*x & mask) {
        return false;
      }
    }
    return true;
  }

  template <typename Ntk>
  inline int CsoSimulator<Ntk>::FindOne(int n, citr x, word mask) const {
    if(mask == one) {
      for(int i = 0; i < n; i++, x++) {
        if(*x) {
          return i;
        }
      }
    } else {
      for(int i = 0; i < n - 1; i++, x++) {
        if(*x) {
          return i;
        }
      }
      if(*x & mask) {
        return n - 1;
      }
    }
    return -1;
  }

  template <typename Ntk>
  inline int CsoSimulator<Ntk>::FindOneBit(word x) const {
    assert(x);
    return __builtin_clzll(x);
  }
  
  template <typename Ntk>
  inline bool CsoSimulator<Ntk>::IsEq(int n, citr x, citr y, bool c, word mask) const {
    if(!c) {
      if(mask == one) {
        for(int i = 0; i < n; i++, x++, y++) {
          if(*x != *y) {
            return false;
          }
        }
      } else {
        for(int i = 0; i < n - 1; i++, x++, y++) {
          if(*x != *y) {
            return false;
          }
        }
        if((*x ^ *y) & mask) {
          return false;
        }
      }
    } else {
      if(mask == one) {
        for(int i = 0; i < n; i++, x++, y++) {
          if(*x != ~*y) {
            return false;
          }
        }
      } else {
        for(int i = 0; i < n - 1; i++, x++, y++) {
          if(*x != ~*y) {
            return false;
          }
        }
        if((*x ^ ~*y) & mask) {
          return false;
        }
      }
    }
    return true;
  }

  template <typename Ntk>
  inline void CsoSimulator<Ntk>::Print(int n, citr x) const {
    std::cout << std::bitset<64>(*x);
    x++;
    for(int i = 1; i < n; i++, x++) {
      std::cout << std::endl << std::string(10, ' ') << std::bitset<64>(*x);
    }
  }
  
  /* }}} */

  /* {{{ Callback */
  
  template <typename Ntk>
  void CsoSimulator<Ntk>::ActionCallback(Action const &action) {
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
      break;
    case TRIVIAL_DECOMPOSE:
      if(fInitialized) {
        vValues.resize(nWords * pNtk->GetNumNodes());
        SimulateNode(pNtk, vValues, action.fi);
        // time of this simulation is not measured for simplicity sake
      }
      break;
    case SORT_FANINS:
      break;
    case READ:
      // Keep backups; it may be good to keep the word vector allocated
      fInitialized = false;
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
        fInitialized = false;
      }
      break;
    case POP_BACK:
      // Keep backups; it may be good to keep the word vector allocated
      break;
    case INSERT:
      // Keep backups; it may be good to keep the word vector allocated
      fInitialized = false;
      break;
    default:
      assert(0);
    }
  }
  
  /* }}} */

  /* {{{ Topology */
  
  template <typename Ntk>
  inline unsigned CsoSimulator<Ntk>::StartTraversal(int n) {
    do {
      for(int i = 0; i < n; i++) {
        iTrav++;
        if(iTrav == 0) {
          vTrav.clear();
          if(pNtk->GetCond()) {
            vTravCond.clear();
          }
          break;
        }
      }
    } while(iTrav == 0);
    vTrav.resize(pNtk->GetNumNodes());
    if(pNtk->GetCond()) {
      vTravCond.resize(pNtk->GetCond()->GetNumNodes());
    }
    return iTrav - n + 1;
  }

  /* }}} */
  
  /* {{{ Simulation */
  
  template <typename Ntk>
  void CsoSimulator<Ntk>::SimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) const {
    itr x = v.end();
    itr y = v.begin() + id * nWords;
    bool cx = false;
    switch(pNtk_->GetNodeType(id)) {
    case AND:
      pNtk_->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nWords;
          cx = c;
        } else {
          And(nWords - nMaskedWords, y, x, v.begin() + fi * nWords, cx, c);
          x = y;
          cx = false;
        }
      });
      if(x == v.end()) {
        Fill(nWords - nMaskedWords, y);
      } else if(x != y) {
        Copy(nWords - nMaskedWords, y, x, cx);
      }
      break;
    default:
      assert(0);
    }
  }
      
  template <typename Ntk>
  bool CsoSimulator<Ntk>::ResimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) {
    itr x = v.end();
    bool cx = false;
    switch(pNtk_->GetNodeType(id)) {
    case AND:
      pNtk_->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nWords;
          cx = c;
        } else {
          And(nWords - nMaskedWords, tmp.begin(), x, v.begin() + fi * nWords, cx, c);
          x = tmp.begin();
          cx = false;
        }
      });
      if(x == v.end()) {
        Fill(nWords - nMaskedWords, tmp.begin());
      } else if(x != tmp.begin()) {
        Copy(nWords - nMaskedWords, tmp.begin(), x, cx);
      }
      break;
    default:
      assert(0);
    }
    itr y = v.begin() + id * nWords;
    if(IsEq(nWords - nMaskedWords, y, tmp.begin(), false, wLastMask)) {
      return false;
    }
    Copy(nWords - nMaskedWords, y, tmp.begin(), false);
    return true;
  }
  
  template <typename Ntk>
  void CsoSimulator<Ntk>::Simulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "simulating" << std::endl;
    }
    pNtk->ForEachInt([&](int id) {
      SimulateNode(pNtk, vValues, id);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nWords - nMaskedWords, vValues.begin() + id * nWords);
        std::cout << std::endl;
      }
    });
    /*
    vPoValues.resize(pNtk->GetNumPos());
    int index = 0;
    pNtk->ForEachPoDriver([&](int fi, bool c) {
      vPoValues[index].resize(nWords);
      Copy(nWords - nMaskedWords, vPoValues[index].begin(), vValues.begin() + fi * nWords, c);
      index++;
    });
    */
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }
  
  template <typename Ntk>
  void CsoSimulator<Ntk>::Resimulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "resimulating" << std::endl;
    }
    pNtk->ForEachTfosUpdate(sUpdates, false, [&](int fo) {
      bool fUpdated = ResimulateNode(pNtk, vValues, fo);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << fo << ": ";
        Print(nWords - nMaskedWords, vValues.begin() + fo * nWords);
        std::cout << std::endl;
      }
      return fUpdated;
    });
    /* alternative version that updates entire TFO
    pNtk->ForEachTfos(sUpdates, false, [&](int fo) {
      SimulateNode(vValues, fo);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << fo << ": ";
        Print(nWords - nMaskedWords, vValues.begin() + fo * nWords);
        std::cout << std::endl;
      }
    });
    */
    /*
    int index = 0;
    pNtk->ForEachPoDriver([&](int fi, bool c){
      assert(IsEq(nWords - nMaskedWords, vPoValues[index].begin(), vValues.begin() + fi * nWords, c));
      index++;
    });
    */
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }

  /* }}} */

  /* {{{ Stimuli */
  
  template <typename Ntk>
  void CsoSimulator<Ntk>::ReadStimuli(Pattern *pPat) {
    nWords = pPat->GetNumWords();
    vValues.resize(nWords * pNtk->GetNumNodes());
    pNtk->ForEachPiIdx([&](int index, int id) {
      Copy(nWords, vValues.begin() + id * nWords, pPat->GetIterator(index), false);
    });
    wLastMask = pPat->GetLastMask();
    nMaskedWords = 0;
    fGenerated = true;
  }
  
  /* }}} */

  /* {{{ Careset computation */
  
  template <typename Ntk>
  void CsoSimulator<Ntk>::ComputeCare(int id) {
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
      Fill(nWords - nMaskedWords, care.begin());
      if(nVerbose) {
        std::cout << "care " << std::setw(3) << target << ": ";
        Print(nWords - nMaskedWords, care.begin());
        std::cout << std::endl;
      }
      durationCare += Duration(timeStart, GetCurrentTime());
      return;
    }
    vValues2.resize(nWords * pNtk->GetNumNodes());
    StartTraversal();
    Copy(nWords - nMaskedWords, vValues2.begin() + target * nWords, vValues.begin() + target * nWords, true);
    vTrav[target] = iTrav;
    pNtk->ForEachTfo(target, false, [&](int id) {
      itr x = vValues2.end();
      itr y = vValues2.begin() + id * nWords;
      bool cx = false;
      switch(pNtk->GetNodeType(id)) {
      case AND:
        pNtk->ForEachFanin(id, [&](int fi, bool c) {
          if(x == vValues2.end()) {
            if(vTrav[fi] != iTrav) {
              x = vValues.begin() + fi * nWords;
            } else {
              x = vValues2.begin() + fi * nWords;
            }
            cx = c;
          } else {
            if(vTrav[fi] != iTrav) {
              And(nWords - nMaskedWords, y, x, vValues.begin() + fi * nWords, cx, c);
            } else {
              And(nWords - nMaskedWords, y, x, vValues2.begin() + fi * nWords, cx, c);
            }
            x = y;
            cx = false;
          }
        });
        if(x == vValues2.end()) {
          Fill(nWords - nMaskedWords, y);
        } else if(x != y) {
          Copy(nWords - nMaskedWords, y, x, cx);
        }
      break;
      default:
        assert(0);
      }
      vTrav[id] = iTrav;
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nWords - nMaskedWords, vValues2.begin() + id * nWords);
        std::cout << std::endl;
      }
    });
    Clear(nWords - nMaskedWords, care.begin());
    pNtk->ForEachPoDriver([&](int fi) {
      assert(fi != target);
      if(vTrav[fi] == iTrav) { // skip unaffected POs
        for(int i = 0; i < nWords - nMaskedWords; i++) {
          care[i] = care[i] | (vValues[fi * nWords + i] ^ vValues2[fi * nWords + i]);
        }
      }
    });
    if(nVerbose) {
      std::cout << "care " << std::setw(3) << target << ": ";
      Print(nWords - nMaskedWords, care.begin());
      std::cout << std::endl;
    }
    durationCare += Duration(timeStart, GetCurrentTime());
  }

  /* }}} */

  /* {{{ Preparation */

  template <typename Ntk>
  void CsoSimulator<Ntk>::Initialize() {
    assert(fGenerated);
    vValues.resize(nWords * pNtk->GetNumNodes());
    // TODO: maybe reset updates and others as well
    Simulate();
    fInitialized = true;
  }

  /* }}} */
  
  /* {{{ Save & load */

  template <typename Ntk>
  void CsoSimulator<Ntk>::Save(int slot) {
    if(slot >= int_size(vBackups)) {
      vBackups.resize(slot + 1);
    }
    vBackups[slot].fInitialized = fInitialized;
    if(!fInitialized) {
      return;
    }
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
    target = vBackups[slot].target; // assigned to -1 when careset needs updating
    vBackups[slot].vValues = vValues;
  }

  template <typename Ntk>
  void CsoSimulator<Ntk>::Load(int slot) {
    assert(slot < int_size(vBackups));
    fUpdate = false;
    sUpdates.clear();
    if(!vBackups[slot].fInitialized) {
      target = -1;
      fInitialized = false;
      return;
    }
    target = vBackups[slot].target;
    care = vBackups[slot].care;
    vValues = vBackups[slot].vValues;
  }

  /* }}} */
  
  /* {{{ Constructors */
  
  template <typename Ntk>
  CsoSimulator<Ntk>::CsoSimulator() :
    pNtk(NULL),
    nVerbose(0),
    fSave(false),
    fGenerated(false),
    fInitialized(false),
    nWords(0),
    wLastMask(one),
    nMaskedWords(0),
    target(-1),
    iTrav(0),
    fUpdate(false) {
    ResetSummary();
  }
  
  template <typename Ntk>
  CsoSimulator<Ntk>::CsoSimulator(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nSimulatorVerbose),
    fSave(pPar->fSave),
    fGenerated(false),
    fInitialized(false),
    nWords(0),
    wLastMask(one),
    nMaskedWords(0),
    target(-1),
    iTrav(0),
    fUpdate(false) {
    care.resize(nWords);
    tmp.resize(nWords);
    ResetSummary();
  }

  template <typename Ntk>
  void CsoSimulator<Ntk>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    if(!fReuse) { // could be just nWords same or not
      fGenerated = false;
    }
    fInitialized = false;
    target = -1;
    fUpdate = false;
    sUpdates.clear();
    pNtk = pNtk_;
    pNtk->AddCallback(std::bind(&CsoSimulator<Ntk>::ActionCallback, this, std::placeholders::_1));
    if(!fGenerated) {
      Pattern *pPat = pNtk->GetPattern();
      assert(pPat);
      ReadStimuli(pPat);
      care.resize(nWords);
      tmp.resize(nWords);
    }
  }

  /* }}} */

  /* {{{ Checks */
  
  template <typename Ntk>
  bool CsoSimulator<Ntk>::CheckRedundancy(int id, int idx) {
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
          And(nWords - nMaskedWords, tmp.begin(), x, vValues.begin() + fi * nWords, cx, c);
          x = tmp.begin();
          cx = false;
        }
      });
      if(x == vValues.end()) {
        x = care.begin();
      } else {
        And(nWords - nMaskedWords, tmp.begin(), x, care.begin(), cx, false);
        x = tmp.begin();
      }
      int fi = pNtk->GetFanin(id, idx);
      bool c = pNtk->GetCompl(id, idx);
      And(nWords - nMaskedWords, tmp.begin(), x, vValues.begin() + fi * nWords, false, !c);
      // TODO: shall we keep track where was the last xor (minimum patterns to drop to remove at least one), probably modify analyzer...
      return IsZero(nWords - nMaskedWords, tmp.begin(), wLastMask);
    }
    default:
      assert(0);
    }
    return false;
  }

  template <typename Ntk>
  bool CsoSimulator<Ntk>::CheckFeasibility(int id, int fi, bool c) {
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
          And(nWords - nMaskedWords, tmp.begin(), x, vValues.begin() + fi * nWords, cx, c);
          x = tmp.begin();
          cx = false;
        }
      });
      if(x == vValues.end()) {
        x = care.begin();
      } else {
        And(nWords - nMaskedWords, tmp.begin(), x, care.begin(), cx, false);
        x = tmp.begin();
      }
      And(nWords - nMaskedWords, tmp.begin(), x, vValues.begin() + fi * nWords, false, !c);
      return IsZero(nWords - nMaskedWords, tmp.begin(), wLastMask);
    }
    default:
      assert(0);
    }
    return false;
  }

  /* }}} */

  /* {{{ Others */

  template <typename Ntk>
  void CsoSimulator<Ntk>::DropLastPattern() {
    wLastMask <<= 1;
    if(!wLastMask) {
      wLastMask = one;
      nMaskedWords++;
    }
    assert(0);
  }
  
  /* }}} */
  
  /* {{{ Summary */
  
  template <typename Ntk>
  void CsoSimulator<Ntk>::ResetSummary() {
    durationSimulation = 0;
    durationCare = 0;
  };
  
  template <typename Ntk>
  summary<int> CsoSimulator<Ntk>::GetStatsSummary() const {
    summary<int> v;
    return v;
  };
  
  template <typename Ntk>
  summary<double> CsoSimulator<Ntk>::GetTimesSummary() const {
    summary<double> v;
    v.emplace_back("sim simulation", durationSimulation);
    v.emplace_back("sim care computation", durationCare);
    return v;
  };
  
  /* }}} */
  
}
