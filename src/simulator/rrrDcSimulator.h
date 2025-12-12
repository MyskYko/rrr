#pragma once

#include <algorithm>
#include <random>
#include <bitset>

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"
#include "extra/rrrPattern.h"

namespace rrr {

  template <typename Ntk>
  class DcSimulator {
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
    std::vector<word> vFs;
    std::vector<word> vGs;
    std::vector<std::vector<word>> vvCs;
    std::vector<word> vValues2; // simulation with an inverter
    std::vector<word> tmp;

    // marks
    unsigned iTrav;
    std::vector<unsigned> vTrav;
    std::vector<unsigned> vTravCond;

    // updates
    bool fUpdate;
    std::vector<bool> vUpdates;
    std::vector<bool> vGUpdates;
    std::vector<bool> vCUpdates;
    std::vector<bool> vVisits;
    std::vector<bool> vWasReconvergent;

    // backups
    std::vector<DcSimulator> vBackups;

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
    bool IsZero(int n, citr x, bool c) const;
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
    bool ComputeReconvergentG(int id);
    bool ComputeG(int id);
    void ComputeC(int id);
    void MspfNode(int id);
    void Mspf(int id, bool fC);

    // preparation
    void Initialize();

    // save & load
    void Save(int slot);
    void Load(int slot);

  public:
    // constructors
    DcSimulator();
    DcSimulator(Parameter const *pPar);
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
  inline void DcSimulator<Ntk>::Clear(int n, itr x) const {
    std::fill(x, x + n, 0);
  }

  template <typename Ntk>
  inline void DcSimulator<Ntk>::Fill(int n, itr x) const {
    std::fill(x, x + n, one);
  }
  
  template <typename Ntk>
  inline void DcSimulator<Ntk>::Copy(int n, itr dst, citr src, bool c) const {
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
  inline void DcSimulator<Ntk>::And(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
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
  inline void DcSimulator<Ntk>::Or(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
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
  inline void DcSimulator<Ntk>::Xor(int n, itr dst, citr src0, citr src1, bool c) const {
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
  inline bool DcSimulator<Ntk>::IsZero(int n, citr x, bool c) const {
    if(!c) {
      for(int i = 0; i < n; i++, x++) {
        if(*x) {
          return false;
        }
      }
    } else {
      for(int i = 0; i < n; i++, x++) {
        if(~*x) {
          return false;
        }
      }
    }
    return true;
  }

  template <typename Ntk>
  inline bool DcSimulator<Ntk>::IsEq(int n, citr x, citr y, bool c) const {
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
  inline void DcSimulator<Ntk>::Print(int n, citr x) const {
    std::cout << std::bitset<64>(*x);
    x++;
    for(int i = 1; i < n; i++, x++) {
      std::cout << std::endl << std::string(10, ' ') << std::bitset<64>(*x);
    }
  }
  
  /* }}} */

  /* {{{ Callback */
  
  template <typename Ntk>
  void DcSimulator<Ntk>::ActionCallback(Action const &action) {
    switch(action.type) {
    case REMOVE_FANIN:
      assert(fInitialized);
      fUpdate = true;
      std::fill(vVisits.begin(), vVisits.end(), false);
      vUpdates[action.id] = true;
      vCUpdates[action.id] = true;
      vGUpdates[action.fi] = true;
      Copy(vvCs[action.id].size() - (action.idx + 1) * nWords, vvCs[action.id].begin() + action.idx * nWords, vvCs[action.id].begin() + (action.idx + 1) * nWords, false);
      vvCs[action.id].resize(vvCs[action.id].size() - nWords);
      break;
    case REMOVE_UNUSED:
      if(fInitialized) {
        if(vGUpdates[action.id] || vCUpdates[action.id]) {
          for(int fi: action.vFanins) {
            vGUpdates[fi] = true;
          }
        }
        Clear(nWords, vFs.begin() + action.id * nWords);
        Clear(nWords, vGs.begin() + action.id * nWords);
        vvCs[action.id].clear();
      }
      break;
    case REMOVE_BUFFER:
      if(fInitialized) {
        if(vUpdates[action.id]) {
          fUpdate = true;
          for(int fo: action.vFanouts) {
            vUpdates[fo] = true;
            vCUpdates[fo] = true;
          }
        }
        if(vGUpdates[action.id] || vCUpdates[action.id]) {
          vGUpdates[action.fi] = true;
        }
        Clear(nWords, vFs.begin() + action.id * nWords);
        Clear(nWords, vGs.begin() + action.id * nWords);
        vvCs[action.id].clear();
      }
      break;
    case REMOVE_CONST:
      if(fInitialized) {
        if(vUpdates[action.id]) {
          fUpdate = true;
          for(int fo: action.vFanouts) {
            vUpdates[fo] = true;
            vCUpdates[fo] = true;
          }
        }
        for(int fi: action.vFanins) {
          vGUpdates[fi] = true;
        }
      }
      Clear(nWords, vFs.begin() + action.id * nWords);
      Clear(nWords, vGs.begin() + action.id * nWords);
      vvCs[action.id].clear();
      break;
    case ADD_FANIN:
      assert(fInitialized);
      fUpdate = true;
      std::fill(vVisits.begin(), vVisits.end(), false);
      vUpdates[action.id] = true;
      vCUpdates[action.id] = true;
      vvCs[action.id].insert(vvCs[action.id].begin() + action.idx * nWords, nWords, 0);
      break;
    case TRIVIAL_COLLAPSE:
      if(fInitialized) {
        if(vGUpdates[action.fi] || vCUpdates[action.fi]) {
          vCUpdates[action.id] = true;
        }
        itr it = vvCs[action.id].begin() + action.idx * nWords;
        it = vvCs[action.id].erase(it, it + nWords);
        for(int idx: action.vIndices) {
          it = vvCs[action.id].insert(it,  vvCs[action.fi].begin() + idx * nWords, vvCs[action.fi].begin() + (idx + 1) * nWords);
          it += nWords;
        }
        vvCs[action.fi].clear();
        Clear(nWords, vFs.begin() + action.fi * nWords);
        Clear(nWords, vGs.begin() + action.fi * nWords);
      }
      break;
    case TRIVIAL_DECOMPOSE:
      if(fInitialized) {
        vFs.resize(pNtk->GetNumNodes() * nWords);
        vGs.resize(pNtk->GetNumNodes() * nWords);
        vvCs.resize(pNtk->GetNumNodes());
        vUpdates.resize(pNtk->GetNumNodes());
        vGUpdates.resize(pNtk->GetNumNodes());
        vCUpdates.resize(pNtk->GetNumNodes());
        vVisits.resize(pNtk->GetNumNodes());
        vWasReconvergent.resize(pNtk->GetNumNodes());
        SimulateNode(pNtk, vFs, action.fi);
        // time of this simulation is not measured for simplicity sake
        itr it = vvCs[action.id].begin() + action.idx * nWords;
        vvCs[action.fi].insert(vvCs[action.fi].begin(), it, vvCs[action.id].end());
        vvCs[action.id].erase(it, vvCs[action.id].end());
        assert(vvCs[action.id].size() == action.idx * nWords);
        if(!vGUpdates[action.id] && !vCUpdates[action.id]) {
          // recompute here only when updates are unlikely to happen
          if(IsZero(nWords, vGs.begin() + action.id * nWords, true)) {
            Fill(nWords, vGs.begin() + action.fi * nWords);
          } else {
            Fill(nWords, tmp.begin());
            for(int idx2 = 0; idx2 < action.idx; idx2++) {
              int fi = pNtk->GetFanin(action.id, idx2);
              bool c = pNtk->GetCompl(action.id, idx2);
              And(nWords, tmp.begin(), tmp.begin(), vFs.begin() + fi * nWords, false, c);
            }
            Or(nWords, vGs.begin() + action.fi * nWords, tmp.begin(), vGs.begin() + action.id * nWords, true, false);
          }
        } else {
          // otherwise mark the node for future update
          vCUpdates[action.id] = true;
        }
        vvCs[action.id].resize((action.idx + 1) * nWords);
        Copy(nWords, vvCs[action.id].begin() + action.idx * nWords, vGs.begin() + action.fi * nWords, false);
        vUpdates[action.fi] = false;
        vGUpdates[action.fi] = false;
        vCUpdates[action.fi] = false;
        vVisits[action.fi] = false;
        vWasReconvergent[action.fi] = false;
      }
      break;
    case SORT_FANINS:
      if(fInitialized) {
        std::vector<word> vCs = vvCs[action.id];
        vvCs[action.id].clear();
        for(int index: action.vIndices) {
          for(int i = 0; i < nWords; i++) {
            vvCs[action.id].push_back(vCs[index * nWords + i]);
          }
        }
      }
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
  inline unsigned DcSimulator<Ntk>::StartTraversal(int n) {
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
  void DcSimulator<Ntk>::SimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) const {
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
  bool DcSimulator<Ntk>::ResimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) {
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
  void DcSimulator<Ntk>::Simulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "simulating" << std::endl;
    }
    pNtk->ForEachInt([&](int id) {
      SimulateNode(pNtk, vFs, id);
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id << ": ";
        Print(nWords, vFs.begin() + id * nWords);
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
  void DcSimulator<Ntk>::Resimulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "resimulating" << std::endl;
    }
    pNtk->ForEachInt([&](int id) {
      if(vUpdates[id]) {
        if(ResimulateNode(pNtk, vFs, id)) {
          pNtk->ForEachFanout(id, false, [&](int fo) {
            vUpdates[fo] = true;
            vCUpdates[fo] = true;
          });
        }
        vUpdates[id] = false;
      }
    });
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
  void DcSimulator<Ntk>::GenerateExhaustiveStimuli() {
    if(nVerbose) {
      std::cout << "generating exhaustive stimuli" << std::endl;
    }
    assert(pNtk->GetNumPis() < 30);
    if(pNtk->GetNumPis() <= 6) {
      nWords = 1;
    } else {
      nWords = 1 << (pNtk->GetNumPis() - 6);
    }
    vFs.resize(pNtk->GetNumNodes() * nWords);
    pNtk->ForEachPiIdx([&](int index, int id) {
      if(index < 6) {
        itr it = vFs.begin() + id * nWords;
        for(int i = 0; i < nWords; i++, it++) {
          *it = basepats[index];
        }
      } else {
        itr it = vFs.begin() + id * nWords;
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
        Print(nWords, vFs.begin() + id * nWords);
        std::cout << std::endl;
      }
    });
    fGenerated = true;
  }
  
  /* }}} */

  /* {{{ Careset computation */

  template <typename Ntk>
  inline bool DcSimulator<Ntk>::ComputeReconvergentG(int id) {
    if(pNtk->IsPoDriver(id)) {
      if(IsZero(nWords, vGs.begin() + id * nWords, false)) {
        return false;
      }
      Clear(nWords, vGs.begin() + id * nWords);
      return true;
    }
    vValues2.resize(nWords * pNtk->GetNumNodes());
    StartTraversal();
    Copy(nWords, vValues2.begin() + id * nWords, vFs.begin() + id * nWords, true);
    vTrav[id] = iTrav;
    pNtk->ForEachTfo(id, false, [&](int id_) {
      itr x = vValues2.end();
      itr y = vValues2.begin() + id_ * nWords;
      bool cx = false;
      switch(pNtk->GetNodeType(id_)) {
      case AND:
        pNtk->ForEachFanin(id_, [&](int fi, bool c) {
          if(x == vValues2.end()) {
            if(vTrav[fi] != iTrav) {
              x = vFs.begin() + fi * nWords;
            } else {
              x = vValues2.begin() + fi * nWords;
            }
            cx = c;
          } else {
            if(vTrav[fi] != iTrav) {
              And(nWords, y, x, vFs.begin() + fi * nWords, cx, c);
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
      vTrav[id_] = iTrav;
      if(nVerbose) {
        std::cout << "node " << std::setw(3) << id_ << ": ";
        Print(nWords, vValues2.begin() + id * nWords);
        std::cout << std::endl;
      }
    });
    Clear(nWords, tmp.begin());
    pNtk->ForEachPoDriver([&](int fi) {
      assert(fi != id);
      if(vTrav[fi] == iTrav) { // skip unaffected POs
        for(int i = 0; i < nWords; i++) {
          tmp[i] = tmp[i] | (vFs[fi * nWords + i] ^ vValues2[fi * nWords + i]);
        }
      }
    });
    if(IsEq(nWords, tmp.begin(), vGs.begin() + id * nWords, true)) {
      return false;
    }
    Copy(nWords, vGs.begin() + id * nWords, tmp.begin(), true);
    return true;
  }
  
  template <typename Ntk>
  inline bool DcSimulator<Ntk>::ComputeG(int id) {
    if(pNtk->GetNumFanouts(id) == 0) {
      if(IsZero(nWords, vGs.begin() + id * nWords, true)) {
        return false;
      }
      Fill(nWords, vGs.begin() + id * nWords);
      return true;
    }
    Fill(nWords, tmp.begin());
    pNtk->ForEachFanoutRidx(id, true, [&](int fo, int idx) {
      And(nWords, tmp.begin(), tmp.begin(), vvCs[fo].begin() + idx * nWords, false, false);
    });
    if(IsEq(nWords, vGs.begin() + id * nWords, tmp.begin(), false)) {
      return false;
    }
    Copy(nWords, vGs.begin() + id * nWords, tmp.begin(), false);
    return true;
  }

  template <typename Ntk>
  inline void DcSimulator<Ntk>::ComputeC(int id) {
    int nFanins = pNtk->GetNumFanins(id);
    assert(vvCs[id].size() == nFanins * nWords);
    if(IsZero(nWords, vGs.begin() + id * nWords, true)) {
      for(int idx = 0; idx < nFanins; idx++) {
        if(!IsZero(nWords, vvCs[id].begin() + idx * nWords, true)) {
          Fill(nWords, vvCs[id].begin() + idx * nWords);
          int fi = pNtk->GetFanin(id, idx);
          vGUpdates[fi] = true;
        }
      }
      return;
    }
    for(int idx = 0; idx < nFanins; idx++) {
      Fill(nWords, tmp.begin());
      for(int idx2 = 0; idx2 < nFanins; idx2++) {
        if(idx2 != idx) {
          int fi = pNtk->GetFanin(id, idx2);
          bool c = pNtk->GetCompl(id, idx2);
          And(nWords, tmp.begin(), tmp.begin(), vFs.begin() + fi * nWords, false, c);
        }
      }
      Or(nWords, tmp.begin(), tmp.begin(), vGs.begin() + id * nWords, true, false);
      if(!IsEq(nWords, vvCs[id].begin() + idx * nWords, tmp.begin(), false)) {
        Copy(nWords, vvCs[id].begin() + idx * nWords, tmp.begin(), false);
        int fi = pNtk->GetFanin(id, idx);
        vGUpdates[fi] = true;
      }
    }
  }

  template <typename pNtk>
  void DcSimulator<pNtk>::MspfNode(int id) {
    if(vGUpdates[id] || !vVisits[id]) {
      if(pNtk->IsReconvergent(id)) {
        if(nVerbose) {
          std::cout << "computing reconvergent node " << id << " G " << std::endl;
        }
        if(ComputeReconvergentG(id)) {
          vCUpdates[id] = true;
        }
        vWasReconvergent[id] = true;
      } else if(vGUpdates[id] || vWasReconvergent[id]) {
        if(nVerbose) {
          std::cout << "computing node " << id << " G " << std::endl;
        }
        if(ComputeG(id)) {
          vCUpdates[id] = true;
        }
        vWasReconvergent[id] = false;
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
    vVisits[id] = true;
  }

  
  template <typename Ntk>
  void DcSimulator<Ntk>::Mspf(int id, bool fC) {
    if(fUpdate) {
      Resimulate();
      fUpdate = false;
    }
    time_point timeStart = GetCurrentTime();
    if(id != -1) {
      pNtk->ForEachTfoReverse(id, false, [&](int fo) {
        MspfNode(fo);
      });
      bool fCUpdate = vCUpdates[id];
      if(!fC) {
        vCUpdates[id] = false;
      }
      MspfNode(id);
      if(!fC) {
        vCUpdates[id] = fCUpdate;
      }
    } else {
      pNtk->ForEachIntReverse([&](int id) {
        MspfNode(id);
      });
    }
    durationCare += Duration(timeStart, GetCurrentTime());
  }

  /* }}} */

  /* {{{ Preparation */

  template <typename Ntk>
  void DcSimulator<Ntk>::Initialize() {
    assert(fGenerated);
    vFs.resize(pNtk->GetNumNodes() * nWords);
    vGs.clear();
    vGs.resize(pNtk->GetNumNodes() * nWords);
    vvCs.resize(pNtk->GetNumNodes());
    for(int i = 0; i < pNtk->GetNumNodes(); i++) {
      vvCs[i].clear();
    }
    Simulate();
    vUpdates.clear();
    vUpdates.resize(pNtk->GetNumNodes());
    vGUpdates.clear();
    vGUpdates.resize(pNtk->GetNumNodes(), true);
    vCUpdates.clear();
    vCUpdates.resize(pNtk->GetNumNodes(), true);
    vVisits.clear();
    vVisits.resize(pNtk->GetNumNodes());
    vWasReconvergent.clear();
    vWasReconvergent.resize(pNtk->GetNumNodes());
    pNtk->ForEachInt([&](int id) {
      vvCs[id].resize(pNtk->GetNumFanins(id) * nWords);
    });
    pNtk->ForEachPo([&](int id) {
      vvCs[id].resize(nWords);
    });
    // TODO: maybe reset updates and others as well
    fInitialized = true;
  }

  /* }}} */

  /* {{{ Save & load */

  template <typename Ntk>
  void DcSimulator<Ntk>::Save(int slot) {
    if(slot >= int_size(vBackups)) {
      vBackups.resize(slot + 1);
    }
    vBackups[slot].vFs = vFs;
    vBackups[slot].vGs = vGs;
    vBackups[slot].vvCs = vvCs;
    vBackups[slot].fUpdate = fUpdate;
    vBackups[slot].vUpdates = vUpdates;
    vBackups[slot].vGUpdates = vGUpdates;
    vBackups[slot].vCUpdates = vCUpdates;
    vBackups[slot].vVisits = vVisits;
    vBackups[slot].vWasReconvergent = vWasReconvergent;
  }

  template <typename Ntk>
  void DcSimulator<Ntk>::Load(int slot) {
    assert(slot < int_size(vBackups));
    vFs = vBackups[slot].vFs;
    vGs = vBackups[slot].vGs;
    vvCs = vBackups[slot].vvCs;
    fUpdate = vBackups[slot].fUpdate;
    vUpdates = vBackups[slot].vUpdates;
    vGUpdates = vBackups[slot].vGUpdates;
    vCUpdates = vBackups[slot].vCUpdates;
    vVisits = vBackups[slot].vVisits;
    vWasReconvergent = vBackups[slot].vWasReconvergent;
  }

  /* }}} */
  
  /* {{{ Constructors */
  
  template <typename Ntk>
  DcSimulator<Ntk>::DcSimulator() :
    pNtk(NULL),
    nVerbose(0),
    nWords(0),
    fSave(false),
    fGenerated(false),
    fInitialized(false),
    iTrav(0),
    fUpdate(false) {
    ResetSummary();
  }
  
  template <typename Ntk>
  DcSimulator<Ntk>::DcSimulator(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nSimulatorVerbose),
    nWords(0),
    fSave(pPar->fSave),
    fGenerated(false),
    fInitialized(false),
    iTrav(0),
    fUpdate(false) {
    tmp.resize(nWords);
    ResetSummary();
  }

  template <typename Ntk>
  void DcSimulator<Ntk>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    if(!fReuse) { // could be just nWords same or not
      fGenerated = false;
    }
    fInitialized = false;
    fUpdate = false;
    pNtk = pNtk_;
    pNtk->AddCallback(std::bind(&DcSimulator<Ntk>::ActionCallback, this, std::placeholders::_1));
    if(!fGenerated) {
      GenerateExhaustiveStimuli();
      tmp.resize(nWords);
    }
  }

  /* }}} */

  /* {{{ Checks */
  
  template <typename Ntk>
  bool DcSimulator<Ntk>::CheckRedundancy(int id, int idx) {
    if(!fInitialized) {
      Initialize();
    }
    Mspf(id, true);
    switch(pNtk->GetNodeType(id)) {
    case AND: {
      int fi = pNtk->GetFanin(id, idx);
      bool c = pNtk->GetCompl(id, idx);
      Or(nWords, tmp.begin(), vFs.begin() + fi * nWords, vvCs[id].begin() + idx * nWords, c, false);
      return IsZero(nWords, tmp.begin(), true);
    }
    default:
      assert(0);
    }
    return false;
  }

  template <typename Ntk>
  bool DcSimulator<Ntk>::CheckFeasibility(int id, int fi, bool c) {
    if(!fInitialized) {
      Initialize();
    }
    Mspf(id, false);
    switch(pNtk->GetNodeType(id)) {
    case AND: {
      Or(nWords, tmp.begin(), vFs.begin() + id * nWords, vGs.begin() + id * nWords, true, false);
      Or(nWords, tmp.begin(), tmp.begin(), vFs.begin() + fi * nWords, false, c);
      return IsZero(nWords, tmp.begin(), true);
    }
    default:
      assert(0);
    }
    return false;
  }

  /* }}} */

  /* {{{ Summary */
  
  template <typename Ntk>
  void DcSimulator<Ntk>::ResetSummary() {
    durationSimulation = 0;
    durationCare = 0;
  };
  
  template <typename Ntk>
  summary<int> DcSimulator<Ntk>::GetStatsSummary() const {
    summary<int> v;
    return v;
  };
  
  template <typename Ntk>
  summary<double> DcSimulator<Ntk>::GetTimesSummary() const {
    summary<double> v;
    v.emplace_back("sim simulation", durationSimulation);
    v.emplace_back("sim care computation", durationCare);
    return v;
  };
  
  /* }}} */
  
}
