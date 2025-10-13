#pragma once

#include <algorithm>
#include <random>
#include <bitset>

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"
#include "extra/rrrPattern.h"

namespace rrr {

  template <typename Ntk>
  class ExhaustiveSimulator {
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
    int nWords;
    bool fSave;

    // data
    bool fGenerated;
    bool fInitialized;
    int target; // node for which the careset has been computed
    std::vector<word> vValues;
    std::vector<word> vValues2; // simulation with an inverter
    std::vector<word> care; // careset
    std::vector<word> tmp;

    // backups
    std::vector<ExhaustiveSimulator> vBackups;

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
    bool IsZero(int n, citr x) const;
    bool IsEq(int n, citr x, citr y, bool c) const;
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
    void GenerateExhaustiveStimuli();

    // careset computation
    void ComputeCare(int id);

    // preparation
    void Initialize();

    // save & load
    void Save(int slot);
    void Load(int slot);
    
  public:
    // constructors
    ExhaustiveSimulator();
    ExhaustiveSimulator(Parameter const *pPar);
    void AssignNetwork(Ntk *pNtk_, bool fReuse);

    // checks
    bool CheckRedundancy(int id, int idx);
    bool CheckFeasibility(int id, int fi, bool c);

    // summary
    void ResetSummary();
    summary<int> GetStatsSummary() const;
    summary<double> GetTimesSummary() const;
  };


  /* {{{ Vector computations */
  
  template <typename Ntk>
  inline void ExhaustiveSimulator<Ntk>::Clear(int n, itr x) const {
    std::fill(x, x + n, 0);
  }

  template <typename Ntk>
  inline void ExhaustiveSimulator<Ntk>::Fill(int n, itr x) const {
    std::fill(x, x + n, one);
  }
  
  template <typename Ntk>
  inline void ExhaustiveSimulator<Ntk>::Copy(int n, itr dst, citr src, bool c) const {
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
  inline void ExhaustiveSimulator<Ntk>::And(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
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
  inline void ExhaustiveSimulator<Ntk>::Or(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
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
  inline void ExhaustiveSimulator<Ntk>::Xor(int n, itr dst, citr src0, citr src1, bool c) const {
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
  inline bool ExhaustiveSimulator<Ntk>::IsZero(int n, citr x) const {
    for(int i = 0; i < n; i++, x++) {
      if(*x) {
        return false;
      }
    }
    return true;
  }

  template <typename Ntk>
  inline bool ExhaustiveSimulator<Ntk>::IsEq(int n, citr x, citr y, bool c) const {
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
  inline void ExhaustiveSimulator<Ntk>::Print(int n, citr x) const {
    std::cout << std::bitset<64>(*x);
    x++;
    for(int i = 1; i < n; i++, x++) {
      std::cout << std::endl << std::string(10, ' ') << std::bitset<64>(*x);
    }
  }
  
  /* }}} */

  /* {{{ Callback */
  
  template <typename Ntk>
  void ExhaustiveSimulator<Ntk>::ActionCallback(Action const &action) {
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
  inline unsigned ExhaustiveSimulator<Ntk>::StartTraversal(int n) {
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
  void ExhaustiveSimulator<Ntk>::SimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) const {
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
  bool ExhaustiveSimulator<Ntk>::ResimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) {
    itr x = v.end();
    bool cx = false;
    switch(pNtk_->GetNodeType(id)) {
    case AND:
      pNtk_->ForEachFanin(id, [&](int fi, bool c) {
        if(x == v.end()) {
          x = v.begin() + fi * nWords;
          cx = c;
        } else {
          And(nWords, tmp.begin(), x, v.begin() + fi * nWords, cx, c);
          x = tmp.begin();
          cx = false;
        }
      });
      if(x == v.end()) {
        Fill(nWords, tmp.begin());
      } else if(x != tmp.begin()) {
        Copy(nWords, tmp.begin(), x, cx); // TODO: unnecessary copy
      }
      break;
    default:
      assert(0);
    }
    itr y = v.begin() + id * nWords;
    if(IsEq(nWords, y, tmp.begin(), false)) {
      return false;
    }
    Copy(nWords, y, tmp.begin(), false);
    return true;
  }
  
  template <typename Ntk>
  void ExhaustiveSimulator<Ntk>::Simulate() {
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
    /*
    vPoValues.resize(pNtk->GetNumPos());
    int index = 0;
    pNtk->ForEachPoDriver([&](int fi, bool c) {
      vPoValues[index].resize(nWords);
      Copy(nWords, vPoValues[index].begin(), vValues.begin() + fi * nWords, c);
      index++;
    });
    */
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }
  
  template <typename Ntk>
  void ExhaustiveSimulator<Ntk>::Resimulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "resimulating" << std::endl;
    }
    pNtk->ForEachTfosUpdate(sUpdates, false, [&](int fo) {
      bool fUpdated = ResimulateNode(pNtk, vValues, fo);
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
    /*
    int index = 0;
    pNtk->ForEachPoDriver([&](int fi, bool c){
      assert(IsEq(nWords, vPoValues[index].begin(), vValues.begin() + fi * nWords, c));
      index++;
    });
    */
    durationSimulation += Duration(timeStart, GetCurrentTime());
  }

  /* }}} */

  /* {{{ Stimuli */
  
  template <typename Ntk>
  void ExhaustiveSimulator<Ntk>::GenerateExhaustiveStimuli() {
    if(nVerbose) {
      std::cout << "generating exhaustive stimuli" << std::endl;
    }
    assert(pNtk->GetNumPis() < 30);
    if(pNtk->GetNumPis() <= 6) {
      nWords = 1;
    } else {
      nWords = 1 << (pNtk->GetNumPis() - 6);
    }
    vValues.resize(nWords * pNtk->GetNumNodes());
    pNtk->ForEachPiIdx([&](int index, int id) {
      if(index < 6) {
        itr it = vValues.begin() + id * nWords;
        for(int i = 0; i < nWords; i++, it++) {
          *it = basepats[index];
        }
      } else {
        itr it = vValues.begin() + id * nWords;
        for(int i = 0; i < nWords;) {
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
        Print(nWords, vValues.begin() + id * nWords);
        std::cout << std::endl;
      }
    });
    fGenerated = true;
  }
  
  /* }}} */

  /* {{{ Careset computation */
  
  template <typename Ntk>
  void ExhaustiveSimulator<Ntk>::ComputeCare(int id) {
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
    Clear(nWords, care.begin());
    pNtk->ForEachPoDriver([&](int fi) {
      assert(fi != target);
      if(vTrav[fi] == iTrav) { // skip unaffected POs
        for(int i = 0; i < nWords; i++) {
          care[i] = care[i] | (vValues[fi * nWords + i] ^ vValues2[fi * nWords + i]);
        }
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
  void ExhaustiveSimulator<Ntk>::Initialize() {
    assert(fGenerated);
    vValues.resize(nWords * pNtk->GetNumNodes());
    // TODO: maybe reset updates and others as well
    Simulate();
    fInitialized = true;
  }

  /* }}} */
  
  /* {{{ Save & load */

  template <typename Ntk>
  void ExhaustiveSimulator<Ntk>::Save(int slot) {
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
  void ExhaustiveSimulator<Ntk>::Load(int slot) {
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
  ExhaustiveSimulator<Ntk>::ExhaustiveSimulator() :
    pNtk(NULL),
    nVerbose(0),
    nWords(0),
    fSave(false),
    fGenerated(false),
    fInitialized(false),
    target(-1),
    iTrav(0),
    fUpdate(false) {
    ResetSummary();
  }
  
  template <typename Ntk>
  ExhaustiveSimulator<Ntk>::ExhaustiveSimulator(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nSimulatorVerbose),
    nWords(0),
    fSave(pPar->fSave),
    fGenerated(false),
    fInitialized(false),
    target(-1),
    iTrav(0),
    fUpdate(false) {
    care.resize(nWords);
    tmp.resize(nWords);
    ResetSummary();
  }

  template <typename Ntk>
  void ExhaustiveSimulator<Ntk>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    if(!fReuse) { // could be just nWords same or not
      fGenerated = false;
    }
    fInitialized = false;
    target = -1;
    fUpdate = false;
    sUpdates.clear();
    pNtk = pNtk_;
    pNtk->AddCallback(std::bind(&ExhaustiveSimulator<Ntk>::ActionCallback, this, std::placeholders::_1));
    if(!fGenerated) {
      GenerateExhaustiveStimuli();
      care.resize(nWords);
      tmp.resize(nWords);
    }
  }

  /* }}} */

  /* {{{ Checks */
  
  template <typename Ntk>
  bool ExhaustiveSimulator<Ntk>::CheckRedundancy(int id, int idx) {
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
  bool ExhaustiveSimulator<Ntk>::CheckFeasibility(int id, int fi, bool c) {
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

  /* {{{ Summary */
  
  template <typename Ntk>
  void ExhaustiveSimulator<Ntk>::ResetSummary() {
    durationSimulation = 0;
    durationCare = 0;
  };
  
  template <typename Ntk>
  summary<int> ExhaustiveSimulator<Ntk>::GetStatsSummary() const {
    summary<int> v;
    return v;
  };
  
  template <typename Ntk>
  summary<double> ExhaustiveSimulator<Ntk>::GetTimesSummary() const {
    summary<double> v;
    v.emplace_back("sim simulation", durationSimulation);
    v.emplace_back("sim care computation", durationCare);
    return v;
  };
  
  /* }}} */
  
}
