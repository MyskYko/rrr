#pragma once

#include <algorithm>
#include <random>
#include <bitset>

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"
#include "extra/rrrPattern.h"

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
    int nWords;
    int nStimuli;
    bool fExSim;
    bool fUseCustomCondition = false;
    std::function<word(std::vector<word> const &, std::vector<word> const &)> CustomCondition;
    bool fUseOriginalPoValues = false;
    // TODO: support last_mask

    // data
    bool fGenerated;
    bool fInitialized;
    int target; // node for which the careset has been computed
    std::vector<word> vValues;
    std::vector<word> vValues2; // simulation with an inverter
    std::vector<word> care; // careset
    std::vector<word> tmp;
    //std::vector<std::vector<word>> vPoValues;
    std::vector<word> vValuesCond;
    std::vector<word> vValuesCond2;
    std::vector<word> vOriginalPoValues;

    // marks
    unsigned iTrav;
    std::vector<unsigned> vTrav;
    std::vector<unsigned> vTravCond;

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
    void Or(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const;
    void Xor(int n, itr dst, citr src0, citr src1, bool c) const;
    bool IsZero(int n, citr x) const;
    bool IsEq(int n, citr x, citr y, bool c) const;
    bool AtMostK(int n, citr x, int k) const;
    void Print(int n, citr x) const;

    // callback
    void ActionCallback(Action const &action);

    // topology
    unsigned StartTraversal(int n = 1);
    
    // simulation
    void SimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) const;
    bool ResimulateNode(Ntk *pNtk_, std::vector<word> &v, int id);
    //void SimulateOneWordNode(std::vector<word> &v, int id, int offset, int to_negate = -1);
    //void SimulatePartNode(std::vector<word> &v, int id, int n, int offset, int to_negate = -1);
    void Simulate();
    void Resimulate();
    //void SimulateOneWord(int offset);

    // generate stimuli
    void GenerateRandomStimuli();
    void ReadStimuli(Pattern *pPat);
    void GenerateExhaustiveStimuli();

    // careset computation
    void ComputeCare(int id);
    //void ComputeCarePart(int nWords_, int offset);

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

    // sdc
    std::vector<word> ComputeSdc(std::vector<int> const &ids);

    // cex
    //void AddCex(std::vector<VarValue> const &vCex);

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
  inline void Simulator2<Ntk>::Or(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
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
  inline bool Simulator2<Ntk>::IsEq(int n, citr x, citr y, bool c) const {
    if(!c) {
      for(int i = 0; i < n; i++, x++, y++) {
	if(*x != *y) {
	  return false;
	}
      }
    } else {
      for(int i = 0; i < n; i++, x++, y++) {
	if(*x != ~*y) {
	  return false;
	}
      }
    }
    return true;
  }

  template <typename Ntk>
  inline bool Simulator2<Ntk>::AtMostK(int n, citr x, int k) const {
    int count = 0;
    for(int i = 0; i < n; i++, x++) {
      count += __builtin_popcountll(*x);
      if(count > k) {
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
          SimulateNode(pNtk, vValues, action.fi);
          // time of this simulation is not measured for simplicity sake
          if(pNtk->GetCond()) { // TODO: this looks unnecessary
            vValuesCond.resize(nStimuli * pNtk->GetCond()->GetNumNodes());
          }
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
    case INSERT:
      fInitialized = false;
      break;
    default:
      assert(0);
    }
  }
  
  /* }}} */

  /* {{{ Topology */
  
  template <typename Ntk>
  inline unsigned Simulator2<Ntk>::StartTraversal(int n) {
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
  void Simulator2<Ntk>::SimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) const {
    itr x = v.end();
    itr y = v.begin() + id * nStimuli;
    bool cx = false;
    switch(pNtk_->GetNodeType(id)) {
    case AND:
      pNtk_->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nStimuli;
          cx = c;
        } else {
          And(nStimuli, y, x, v.begin() + fi * nStimuli, cx, c);
          x = y;
          cx = false;
        }
      });
      if(x == v.end()) {
        Fill(nStimuli, y);
      } else if(x != y) {
        Copy(nStimuli, y, x, cx);
      }
      break;
    default:
      assert(0);
    }
  }
      
  template <typename Ntk>
  bool Simulator2<Ntk>::ResimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) {
    itr x = v.end();
    bool cx = false;
    switch(pNtk_->GetNodeType(id)) {
    case AND:
      pNtk_->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nStimuli;
          cx = c;
        } else {
          And(nStimuli, tmp.begin(), x, v.begin() + fi * nStimuli, cx, c);
          x = tmp.begin();
          cx = false;
        }
      });
      if(x == v.end()) {
        Fill(nStimuli, tmp.begin());
      } else if(x != tmp.begin()) {
        Copy(nStimuli, tmp.begin(), x, cx);
      }
      break;
    default:
      assert(0);
    }
    itr y = v.begin() + id * nStimuli;
    if(IsEq(nStimuli, y, tmp.begin(), false)) {
      return false;
    }
    Copy(nStimuli, y, tmp.begin(), false);
    return true;
  }

  /*
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
  */

  /*
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
  */
  
  template <typename Ntk>
  void Simulator2<Ntk>::Simulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "simulating" << std::endl;
    }
    pNtk->ForEachInt([&](int id) {
      SimulateNode(pNtk, vValues, id);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nStimuli, vValues.begin() + id * nStimuli);
        std::cout << std::endl;
      }
    });
    if(pNtk->GetCond()) {
      Ntk *pCond = pNtk->GetCond();
      int index = 0;
      pNtk->ForEachPoDriver([&](int fi, bool c) {
        Copy(nStimuli, vValuesCond.begin() + pCond->GetPi(index) * nStimuli, vValues.begin() + fi * nStimuli, c);
        index++;
      });
      pCond->ForEachInt([&](int id) {
        SimulateNode(pCond, vValuesCond, id);
        // if(nVerbose) {
        //   std::cout << "node " << std::setw(3) << id << ": ";
        //   Print(nStimuli, vValues.begin() + id * nStimuli);
        //   std::cout << std::endl;
        // }
      });
    }
    /*
    vPoValues.resize(pNtk->GetNumPos());
    int index = 0;
    pNtk->ForEachPoDriver([&](int fi, bool c) {
      vPoValues[index].resize(nStimuli);
      Copy(nStimuli, vPoValues[index].begin(), vValues.begin() + fi * nStimuli, c);
      index++;
    });
    */
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }
  
  template <typename Ntk>
  void Simulator2<Ntk>::Resimulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "resimulating" << std::endl;
    }
    pNtk->ForEachTfosUpdate(sUpdates, false, [&](int fo) {
      bool fUpdated = ResimulateNode(pNtk, vValues, fo);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << fo << ": ";
        Print(nStimuli, vValues.begin() + fo * nStimuli);
        std::cout << std::endl;
      }
      return fUpdated;
    });
    if(pNtk->GetCond()) {
      Ntk *pCond = pNtk->GetCond();
      std::set<int> sUpdatesCond;
      int index = 0;
      pNtk->ForEachPoDriver([&](int fi, bool c){
        if(!IsEq(nStimuli, vValuesCond.begin() + pCond->GetPi(index) * nStimuli, vValues.begin() + fi * nStimuli, c)) {
          Copy(nStimuli, vValuesCond.begin() + pCond->GetPi(index) * nStimuli, vValues.begin() + fi * nStimuli, c);
          pCond->ForEachFanout(pCond->GetPi(index), false, [&](int fo) {
            sUpdatesCond.insert(fo);
          });
        }
        index++;
      });
      pCond->ForEachTfosUpdate(sUpdatesCond, false, [&](int fo) {
        bool fUpdated = ResimulateNode(pCond, vValuesCond, fo);
        // if(nVerbose) {
        //   std::cout << "node " << std::setw(3) << fo << ": ";
        //   Print(nStimuli, vValues.begin() + fo * nStimuli);
        //   std::cout << std::endl;
        // }
        return fUpdated;
      });
    }
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
    /*
    int index = 0;
    pNtk->ForEachPoDriver([&](int fi, bool c){
      assert(IsEq(nStimuli, vPoValues[index].begin(), vValues.begin() + fi * nStimuli, c));
      index++;
    });
    */
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }

  /*
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
  */

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
    if(pNtk->GetCond()) {
      vValuesCond.resize(nStimuli * pNtk->GetCond()->GetNumNodes());
    }
    fGenerated = true;
  }

  template <typename Ntk>
  void Simulator2<Ntk>::GenerateExhaustiveStimuli() {
    if(nVerbose) {
      std::cout << "generating exhaustive stimuli" << std::endl;
    }
    assert(pNtk->GetNumPis() < 30);
    if(pNtk->GetNumPis() <= 6) {
      nStimuli = 1;
    } else {
      nStimuli = 1 << (pNtk->GetNumPis() - 6);
    }
    vValues.resize(nStimuli * pNtk->GetNumNodes());
    pNtk->ForEachPiIdx([&](int index, int id) {
      if(index < 6) {
        itr it = vValues.begin() + id * nStimuli;
        for(int i = 0; i < nStimuli; i++, it++) {
          *it = basepats[index];
        }
      } else {
        itr it = vValues.begin() + id * nStimuli;
        for(int i = 0; i < nStimuli;) {
          for(int j = 0; j < (1 << (index - 6)); i++, j++, it++) {
            *it = 0;
          }
          for(int j = 0; j < (1 << (index - 6)); i++, j++, it++) {
            *it = 0xffffffffffffffffull;
          }
        }
      }
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nStimuli, vValues.begin() + id * nStimuli);
        std::cout << std::endl;
      }
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
    if(!pNtk->GetCond() && !fUseCustomCondition && !fUseOriginalPoValues && pNtk->IsPoDriver(target)) {
      Fill(nStimuli, care.begin());
      if(nVerbose) {
        std::cout << "care " << std::setw(3) << target << ": ";
        Print(nStimuli, care.begin());
        std::cout << std::endl;
      }
      durationCare += Duration(timeStart, GetCurrentTime());
      return;
    }
    // alternative of vValues2 = vValues;
    vValues2.resize(nStimuli * pNtk->GetNumNodes());
    StartTraversal();
    Copy(nStimuli, vValues2.begin() + target * nStimuli, vValues.begin() + target * nStimuli, true);
    vTrav[target] = iTrav;
    pNtk->ForEachTfo(target, false, [&](int id) {
      // alternative of SimulateNode(vValues2, id);
      itr x = vValues2.end();
      itr y = vValues2.begin() + id * nStimuli;
      bool cx = false;
      switch(pNtk->GetNodeType(id)) {
      case AND:
        pNtk->ForEachFanin(id, [&](int fi, bool c) {
          if(x == vValues2.end()) {
            if(vTrav[fi] != iTrav) {
              x = vValues.begin() + fi * nStimuli;
            } else {
              x = vValues2.begin() + fi * nStimuli;
            }
            cx = c;
          } else {
            if(vTrav[fi] != iTrav) {
              And(nStimuli, y, x, vValues.begin() + fi * nStimuli, cx, c);
            } else {
              And(nStimuli, y, x, vValues2.begin() + fi * nStimuli, cx, c);
            }
            x = y;
            cx = false;
          }
        });
        if(x == vValues2.end()) {
          Fill(nStimuli, y);
        } else if(x != y) {
          Copy(nStimuli, y, x, cx);
        }
      break;
      default:
        assert(0);
      }
      vTrav[id] = iTrav;
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nStimuli, vValues2.begin() + id * nStimuli);
        std::cout << std::endl;
      }
    });
    if(pNtk->GetCond()) {
      Ntk *pCond = pNtk->GetCond();
      std::set<int> sUpdatesCond;
      vValuesCond2.resize(nStimuli * pCond->GetNumNodes());
      int index = 0;
      pNtk->ForEachPoDriver([&](int fi, bool c) {
        if(vTrav[fi] == iTrav) {
          vTravCond[pCond->GetPi(index)] = iTrav;
          Copy(nStimuli, vValuesCond2.begin() + pCond->GetPi(index) * nStimuli, vValues2.begin() + fi * nStimuli, c);
          pCond->ForEachFanout(pCond->GetPi(index), false, [&](int fo) {
            sUpdatesCond.insert(fo);
          });
        }
        index++;
      });
      pCond->ForEachTfos(sUpdatesCond, false, [&](int id) {
        itr x = vValuesCond2.end();
        itr y = vValuesCond2.begin() + id * nStimuli;
        bool cx = false;
        switch(pCond->GetNodeType(id)) {
        case AND:
          pCond->ForEachFanin(id, [&](int fi, bool c) {
            if(x == vValuesCond2.end()) {
              if(vTravCond[fi] != iTrav) {
                x = vValuesCond.begin() + fi * nStimuli;
              } else {
                x = vValuesCond2.begin() + fi * nStimuli;
              }
              cx = c;
            } else {
              if(vTravCond[fi] != iTrav) {
                And(nStimuli, y, x, vValuesCond.begin() + fi * nStimuli, cx, c);
              } else {
                And(nStimuli, y, x, vValuesCond2.begin() + fi * nStimuli, cx, c);
              }
              x = y;
              cx = false;
            }
          });
          if(x == vValuesCond2.end()) {
            Fill(nStimuli, y);
          } else if(x != y) {
            Copy(nStimuli, y, x, cx);
          }
          break;
        default:
          assert(0);
        }
        vTravCond[id] = iTrav;
        // if(nVerbose) {
        //   std::cout << "node " << std::setw(3) << id << ": ";
        //   Print(nStimuli, vValues2.begin() + id * nStimuli);
        //   std::cout << std::endl;
        // }
      });
    }
    Clear(nStimuli, care.begin());
    if(pNtk->GetCond()) {
      Ntk *pCond = pNtk->GetCond();
      pCond->ForEachPoDriver([&](int fi) {
        for(int i = 0; i < nStimuli; i++) {
          // skip unaffected POs
          if(vTravCond[fi] == iTrav) {
            care[i] = care[i] | (vValuesCond[fi * nStimuli + i] ^ vValuesCond2[fi * nStimuli + i]);
          }
        }
      });
    } else if(fUseCustomCondition) {
      std::vector<word> vPoValues(pNtk->GetNumPos()), vPoValues2(pNtk->GetNumPos());
      for(int i = 0; i < nStimuli; i++) {
        int index = 0;
        pNtk->ForEachPoDriver([&](int fi, bool c) {
          vPoValues[index] = vValues[fi * nStimuli + i];
          if(vTrav[fi] == iTrav) {
            vPoValues2[index] = vValues2[fi * nStimuli + i];
          } else {
            vPoValues2[index] = vValues[fi * nStimuli + i];
          }
          if(c) {
            vPoValues[index] ^= ~0ull;
            vPoValues2[index] ^= ~0ull;
          }
          index++;
        });
        care[i] = CustomCondition(vPoValues, vPoValues2);
      }
    } else if(fUseOriginalPoValues) {
      int index = 0;
      pNtk->ForEachPoDriver([&](int fi, bool c) { // this is actually expensive for design with many POs
        itr x;
        if(vTrav[fi] == iTrav) {
          x = vValues2.begin() + fi * nStimuli;
        } else {
          x = vValues.begin() + fi * nStimuli;
        }
        Xor(nStimuli, tmp.begin(), vOriginalPoValues.begin() + index * nStimuli, x, c);
        Or(nStimuli, care.begin(), care.begin(), tmp.begin(), false, false);
        index++;
      });
    } else {
      pNtk->ForEachPoDriver([&](int fi) {
        assert(fi != target);
        for(int i = 0; i < nStimuli; i++) {
          // skip unaffected POs
          if(vTrav[fi] == iTrav) {
            care[i] = care[i] | (vValues[fi * nStimuli + i] ^ vValues2[fi * nStimuli + i]);
          }
        }
      });
    }
    if(nVerbose) {
      std::cout << "care " << std::setw(3) << target << ": ";
      Print(nStimuli, care.begin());
      std::cout << std::endl;
    }
    durationCare += Duration(timeStart, GetCurrentTime());
  }

  /*
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
  */

  /* }}} */

  /* {{{ Preparation */

  template <typename Ntk>
  void Simulator2<Ntk>::Initialize() {
    if(!fGenerated) {
      // TODO: reset nStimuli to default here maybe, if such a mechanism that changes nStimuli has been implemneted
      vValues.resize(nStimuli * pNtk->GetNumNodes());
      if(pNtk->GetCond()) {
        vValuesCond.resize(nStimuli * pNtk->GetCond()->GetNumNodes());
      }
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
      if(pNtk->GetCond()) {
        vValuesCond.resize(nStimuli * pNtk->GetCond()->GetNumNodes());
      }
    }
    Simulate();
    if(fUseOriginalPoValues) {
      vOriginalPoValues.resize(nStimuli * pNtk->GetNumPos());
      int index = 0;
      pNtk->ForEachPoDriver([&](int fi, bool c) {
        Copy(nStimuli, vOriginalPoValues.begin() + index * nStimuli, vValues.begin() + fi * nStimuli, c);
        index++;
      });
    }
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
    fExSim(true), // for sdc
    fGenerated(false),
    fInitialized(false),
    target(-1),
    iTrav(0),
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
    fExSim(pPar->fExSim),
    fGenerated(false),
    fInitialized(false),
    target(-1),
    iTrav(0),
    iPivot(0),
    fUpdate(false) {
    care.resize(nWords);
    tmp.resize(nWords);
    ResetSummary();
    CustomCondition = [&](std::vector<word> const &v, std::vector<word> const &v2) {
      int n = pNtk->GetNumPos() / 10;
      word res = 0;
      for(int k = 0; k < 64; k++) {
        std::vector<int> vCounts(10), vCounts2(10);
        for(int j = 0; j < 10; j++) {
          for(int i = 0; i < n; i++) {
            vCounts[j] += (v[i + n * j] >> k) & 1;
            vCounts2[j] += (v2[i + n * j] >> k) & 1;
          }
        }
        if(vCounts != vCounts2) {
          res |= 1ull << k;
        }
      }
      return res;
    };
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
    if(!fGenerated && pPat) {
      ReadStimuli(pPat);
      care.resize(nStimuli);
      tmp.resize(nStimuli);
    }
    if(!fGenerated && fExSim) {
      GenerateExhaustiveStimuli();
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

  /* {{{ Sdc */
  
  template <typename Ntk>
  std::vector<unsigned long long> Simulator2<Ntk>::ComputeSdc(std::vector<int> const &ids) {
    if(!fInitialized) {
      Initialize();
    }
    assert(int_size(ids) < 30);
    int nPatterns = 1 << int_size(ids);
    int nSdcWords = nPatterns >> 6;
    if(nSdcWords == 0) {
      nSdcWords = 1;
    }
    std::vector<word> sdc(nSdcWords);
    for(int i = 0; i < nStimuli; i++) {
      for(int j = 0; j < 64; j++) {
        int pattern = 0;
        for(int k = 0; k < int_size(ids); k++) {
          int id = ids[k];
          itr it = vValues.begin() + id * nStimuli + i;
          if(*it & (1ull << j)) {
            pattern |= 1ull << k;
          }
        }
        int pattern_top = pattern >> 6;
        int pattern_bottom = pattern & 63;
        sdc[pattern_top] |= 1ull << pattern_bottom;
      }
    }
    return sdc;
  }

  /* }}} */
  
  /* {{{ Cex */

  /*
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
    for(; iWord < nStimuli; iWord++) {
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
      if(vCex[idx] == rrrTRUE) {
        vValues[id * nStimuli + iWord] |= mask;
      } else {
        assert(vCex[idx] == rrrFALSE);
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
  */
  
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
