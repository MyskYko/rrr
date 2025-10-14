#pragma once

#include <algorithm>
#include <random>
#include <bitset>

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"
#include "extra/rrrPattern.h"

namespace rrr {

  template <typename Ntk>
  class L1Simulator {
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
    int nRemainder;
    word wLastMask;
    int nCurrent;
    int target; // node for which the careset has been computed
    std::vector<word> vValues;
    std::vector<word> vValues2; // simulation with an inverter
    std::vector<word> care;
    std::vector<word> tmp;
    std::vector<word> tmp2;
    std::vector<word> tmp3;
    std::vector<word> vErrors;
    std::vector<word> vPoValues; // TODO: debug
    
    // backups
    std::vector<L1Simulator> vBackups;

    // marks
    unsigned iTrav;
    std::vector<unsigned> vTrav;

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
    int  Popcount(int n, citr x, word mask) const;
    bool IsEq(int n, citr x, citr y, bool c, word mask) const;
    void Print(int n, citr x) const;

    // callback
    void ActionCallback(Action const &action);

    // topology
    unsigned StartTraversal(int n = 1);
    
    // simulation
    void SimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) const;
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
    L1Simulator();
    L1Simulator(Parameter const *pPar);
    void AssignNetwork(Ntk *pNtk_, bool fReuse);

    // checks
    int CheckRedundancy(int id, int idx);
    int CheckFeasibility(int id, int fi, bool c);

    // others
    int GetDefaultThreshold();
    int GetCurrent();
 
    // summary
    void ResetSummary();
    summary<int> GetStatsSummary() const;
    summary<double> GetTimesSummary() const;
  };


  /* {{{ Vector computations */
  
  template <typename Ntk>
  inline void L1Simulator<Ntk>::Clear(int n, itr x) const {
    std::fill(x, x + n, 0);
  }

  template <typename Ntk>
  inline void L1Simulator<Ntk>::Fill(int n, itr x) const {
    std::fill(x, x + n, one);
  }
  
  template <typename Ntk>
  inline void L1Simulator<Ntk>::Copy(int n, itr dst, citr src, bool c) const {
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
  inline void L1Simulator<Ntk>::And(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
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
  inline void L1Simulator<Ntk>::Or(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
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
  inline void L1Simulator<Ntk>::Xor(int n, itr dst, citr src0, citr src1, bool c) const {
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
  inline bool L1Simulator<Ntk>::IsZero(int n, citr x, word mask) const {
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
  inline int L1Simulator<Ntk>::Popcount(int n, citr x, word mask) const {
    int count = 0;
    if(mask == one) {
      for(int i = 0; i < n; i++, x++) {
        count += __builtin_popcountll(*x);
      }
    } else {
      for(int i = 0; i < n - 1; i++, x++) {
        count += __builtin_popcountll(*x);
      }
      count += __builtin_popcountll(*x & mask);
    }
    return count;
  }
  
  template <typename Ntk>
  inline bool L1Simulator<Ntk>::IsEq(int n, citr x, citr y, bool c, word mask) const {
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
  inline void L1Simulator<Ntk>::Print(int n, citr x) const {
    std::cout << std::bitset<64>(*x);
    x++;
    for(int i = 1; i < n; i++, x++) {
      std::cout << std::endl << std::string(10, ' ') << std::bitset<64>(*x);
    }
  }
  
  /* }}} */

  /* {{{ Callback */
  
  template <typename Ntk>
  void L1Simulator<Ntk>::ActionCallback(Action const &action) {
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
      assert(0); // no proper update on error is implemented
      // Keep backups; it may be good to keep the word vector allocated
      fInitialized = false;
      break;
    case SAVE:
      if(fSave) {
        Save(action.idx);
      }
      break;
    case LOAD:
      assert(0); // no proper update on error is implemented
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
      assert(0); // no proper update on error is implemented
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
  inline unsigned L1Simulator<Ntk>::StartTraversal(int n) {
    do {
      for(int i = 0; i < n; i++) {
        iTrav++;
        if(iTrav == 0) {
          vTrav.clear();
          break;
        }
      }
    } while(iTrav == 0);
    vTrav.resize(pNtk->GetNumNodes());
    return iTrav - n + 1;
  }

  /* }}} */
  
  /* {{{ Simulation */
  
  template <typename Ntk>
  void L1Simulator<Ntk>::SimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) const {
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
          And(nWords, y, x, v.begin() + fi * nWords, cx, c);
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
  void L1Simulator<Ntk>::Simulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "simulating" << std::endl;
    }
    pNtk->ForEachInt([&](int id) {
      SimulateNode(pNtk, vValues, id);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nWords, vValues.begin() + id * nWords);
        std::cout << std::endl;
      }
    });
    pNtk->ForEachPo([&](int id) {
      int fi = pNtk->GetFanin(id, 0);
      bool c = pNtk->GetCompl(id, 0);
      Copy(nWords, vValues.begin() + id * nWords, vValues.begin() + fi * nWords, c);
    });

    // TODO: debug
    vPoValues.resize(pNtk->GetNumPos() * nWords);
    pNtk->ForEachPoDriverIdx([&](int idx, int fi, bool c) {
      Copy(nWords, vPoValues.begin() + idx * nWords, vValues.begin() + fi * nWords, c);
    });

    durationSimulation += Duration(timeStart, GetCurrentTime());
  }
  
  template <typename Ntk>
  void L1Simulator<Ntk>::Resimulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "resimulating" << std::endl;
    }
    bool fPoChanged = false;
    pNtk->ForEachTfosUpdate(sUpdates, true, [&](int fo) {
      bool fUpdated = false;
      {
        itr x = vValues.end();
        bool cx = false;
        switch(pNtk->GetNodeType(fo)) {
        case AND:
          pNtk->ForEachFanin(fo, [&](int fi, bool c) {
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
            Fill(nWords, tmp.begin());
            x = tmp.begin();
          }
          break;
        case PO: {
          int fi = pNtk->GetFanin(fo, 0);
          x = vValues.begin() + fi * nWords;
          cx = pNtk->GetCompl(fo, 0);
          break;
        }
        default:
          assert(0);
        }
        itr y = vValues.begin() + fo * nWords;
        if(!IsEq(nWords, y, x, cx, wLastMask)) {
          if(pNtk->IsPo(fo)) {
            int idx = pNtk->GetPoIndex(fo);
            Xor(nWords, tmp2.begin(), y, x, cx);
            And(nWords, tmp3.begin(), vErrors.begin() + idx * nWords, tmp2.begin(), true, false); // !e & g ... increase
            nCurrent += Popcount(nWords, tmp3.begin(), wLastMask);
            And(nWords, tmp3.begin(), vErrors.begin() + idx * nWords, tmp2.begin(), false, false); // e & g ... decrease
            nCurrent -= Popcount(nWords, tmp3.begin(), wLastMask);
            Xor(nWords, vErrors.begin() + idx * nWords, vErrors.begin() + idx * nWords, tmp2.begin(), false);
            fPoChanged = true;
          }
          Copy(nWords, y, x, cx);
          fUpdated = true;
        }
      }
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
    if(fPoChanged) {
      
      // TODO: debug
      pNtk->ForEachPoDriverIdx([&](int idx, int fi, bool c){
        Xor(nWords, tmp.begin(), vPoValues.begin() + idx * nWords, vValues.begin() + fi * nWords, c);
        assert(IsEq(nWords, vErrors.begin() + idx * nWords, tmp.begin(), false, wLastMask));
      });
      int nCurrent_ = 0;
      for(int idx = 0; idx < pNtk->GetNumPos(); idx++) {
        nCurrent_ += Popcount(nWords, vErrors.begin() + idx * nWords, wLastMask);
      }
      assert(nCurrent == nCurrent_);

    }
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }

  /* }}} */

  /* {{{ Stimuli */
  
  template <typename Ntk>
  void L1Simulator<Ntk>::ReadStimuli(Pattern *pPat) {
    nWords = pPat->GetNumWords();
    vValues.resize(nWords * pNtk->GetNumNodes());
    pNtk->ForEachPiIdx([&](int index, int id) {
      Copy(nWords, vValues.begin() + id * nWords, pPat->GetIterator(index), false);
    });
    nRemainder = pPat->GetNumRemainder();
    wLastMask = pPat->GetLastMask();
    fGenerated = true;
  }
  
  /* }}} */

  /* {{{ Careset computation */
  
  template <typename Ntk>
  void L1Simulator<Ntk>::ComputeCare(int id) {
    if(sUpdates.empty() && id == target) {
      if(fUpdate) {
        sUpdates.insert(target);
        Resimulate();
        sUpdates.clear();
        fUpdate = false;
      }
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
    vValues2.resize(nWords * pNtk->GetNumNodes());
    StartTraversal();
    Copy(nWords, vValues2.begin() + target * nWords, vValues.begin() + target * nWords, true);
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
              And(nWords, y, x, vValues.begin() + fi * nWords, cx, c);
            } else {
              And(nWords, y, x, vValues2.begin() + fi * nWords, cx, c);
            }
            x = y;
            cx = false;
          }
        });
        if(x == vValues2.end()) {
          Fill(nWords, y);
        } else if(x != y) {
          Copy(nWords, y, x, cx);
        }
      break;
      default:
        assert(0);
      }
      vTrav[id] = iTrav;
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nWords, vValues2.begin() + id * nWords);
        std::cout << std::endl;
      }
    });
    pNtk->ForEachPoDriverIdx([&](int idx, int fi) {
      if(vTrav[fi] == iTrav) { // skip unaffected POs
        Xor(nWords, care.begin() + idx * nWords, vValues.begin() + fi * nWords, vValues2.begin() + fi * nWords, 0);
        if(nVerbose) {
          std::cout << "care " << std::setw(3) << target << " po " << idx << ": ";
          Print(nWords, care.begin() + idx * nWords);
          std::cout << std::endl;
        }
      }
    });
    durationCare += Duration(timeStart, GetCurrentTime());
  }

  /* }}} */

  /* {{{ Preparation */

  template <typename Ntk>
  void L1Simulator<Ntk>::Initialize() {
    assert(fGenerated);
    vValues.resize(nWords * pNtk->GetNumNodes());
    // TODO: maybe reset updates and others as well
    Simulate();
    fInitialized = true;
  }

  /* }}} */
  
  /* {{{ Save & load */

  template <typename Ntk>
  void L1Simulator<Ntk>::Save(int slot) {
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
  void L1Simulator<Ntk>::Load(int slot) {
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
  L1Simulator<Ntk>::L1Simulator() :
    pNtk(NULL),
    nVerbose(0),
    fSave(false),
    fGenerated(false),
    fInitialized(false),
    nWords(0),
    nRemainder(0),
    wLastMask(one),
    nCurrent(0),
    target(-1),
    iTrav(0),
    fUpdate(false) {
    ResetSummary();
  }
  
  template <typename Ntk>
  L1Simulator<Ntk>::L1Simulator(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nSimulatorVerbose),
    fSave(pPar->fSave),
    fGenerated(false),
    fInitialized(false),
    nWords(0),
    nRemainder(0),
    wLastMask(one),
    nCurrent(0),
    target(-1),
    iTrav(0),
    fUpdate(false) {
    tmp.resize(nWords);
    ResetSummary();
  }

  template <typename Ntk>
  void L1Simulator<Ntk>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    if(!fReuse) { // could be just nWords same or not
      fGenerated = false;
    }
    fInitialized = false;
    target = -1;
    fUpdate = false;
    sUpdates.clear();
    pNtk = pNtk_;
    pNtk->AddCallback(std::bind(&L1Simulator<Ntk>::ActionCallback, this, std::placeholders::_1));
    if(!fGenerated) {
      Pattern *pPat = pNtk->GetPattern();
      assert(pPat);
      ReadStimuli(pPat);
      care.resize(nWords * pNtk->GetNumPos());
      tmp.resize(nWords);
      tmp2.resize(nWords);
      tmp3.resize(nWords);
      vErrors.resize(nWords * pNtk->GetNumPos());
    }
  }

  /* }}} */

  /* {{{ Checks */
  
  template <typename Ntk>
  int L1Simulator<Ntk>::CheckRedundancy(int id, int idx) {
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
      int fi = pNtk->GetFanin(id, idx);
      bool c = pNtk->GetCompl(id, idx);
      if(x == vValues.end()) {
        x = vValues.begin() + fi * nWords;
        cx = !c;
      } else {
        And(nWords, tmp.begin(), x, vValues.begin() + fi * nWords, cx, !c);
        x = tmp.begin();
        cx = false;
      }
      int nDiff = 0;
      pNtk->ForEachPoDriverIdx([&](int idx, int fi) {
        if(vTrav[fi] == iTrav) { // affected POs
          And(nWords, tmp2.begin(), x, care.begin() + idx * nWords, cx, false); // g
          And(nWords, tmp3.begin(), vErrors.begin() + idx * nWords, tmp2.begin(), true, false); // !e & g ... increase
          nDiff+= Popcount(nWords, tmp3.begin(), wLastMask);
          And(nWords, tmp3.begin(), vErrors.begin() + idx * nWords, tmp2.begin(), false, false); // e & g ... decrease
          nDiff -= Popcount(nWords, tmp3.begin(), wLastMask);
        }
      });

      // TODO: debug
      int nCurrent_ = 0;
      pNtk->ForEachPoDriverIdx([&](int idx, int fi) {
        if(vTrav[fi] == iTrav) { // affected POs
          And(nWords, tmp2.begin(), x, care.begin() + idx * nWords, cx, false);
          Xor(nWords, tmp2.begin(), tmp2.begin(), vErrors.begin() + idx * nWords, false); // f = e ^ g
          nCurrent_ += Popcount(nWords, tmp2.begin(), wLastMask);
        } else {
          nCurrent_ += Popcount(nWords, vErrors.begin() + idx * nWords, wLastMask);
        }
      });
      assert(nCurrent_ == nCurrent + nDiff);
      
      return nCurrent + nDiff;
    }
    default:
      assert(0);
    }
    return false;
  }

  template <typename Ntk>
  int L1Simulator<Ntk>::CheckFeasibility(int id, int fi, bool c) {
    assert(0);
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
        //x = care.begin();
      } else {
        //And(nWords, tmp.begin(), x, care.begin(), cx, false);
        x = tmp.begin();
      }
      And(nWords, tmp.begin(), x, vValues.begin() + fi * nWords, false, !c);
      return IsZero(nWords, tmp.begin(), wLastMask);
    }
    default:
      assert(0);
    }
    return false;
  }

  /* }}} */

  /* {{{ Others */

  template <typename Ntk>
  int L1Simulator<Ntk>::GetDefaultThreshold() {
    return 0;
  }

  template <typename Ntk>
  int L1Simulator<Ntk>::GetCurrent() {
    if(!fInitialized) {
      Initialize();
    }
    if(!sUpdates.empty()) {
      Resimulate();
      sUpdates.clear();
    }
    return nCurrent;
  }
  
  /* }}} */
  
  /* {{{ Summary */
  
  template <typename Ntk>
  void L1Simulator<Ntk>::ResetSummary() {
    durationSimulation = 0;
    durationCare = 0;
  };
  
  template <typename Ntk>
  summary<int> L1Simulator<Ntk>::GetStatsSummary() const {
    summary<int> v;
    return v;
  };
  
  template <typename Ntk>
  summary<double> L1Simulator<Ntk>::GetTimesSummary() const {
    summary<double> v;
    v.emplace_back("sim simulation", durationSimulation);
    v.emplace_back("sim care computation", durationCare);
    return v;
  };
  
  /* }}} */
  
}
