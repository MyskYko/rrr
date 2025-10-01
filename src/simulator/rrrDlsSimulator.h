#pragma once

#include <algorithm>
#include <random>
#include <bitset>

#include <torch/torch.h>

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"
#include "extra/rrrPattern.h"

namespace rrr {

  template <typename Ntk>
  class DlsSimulator {
  private:
    // aliases
    using word = unsigned long long;
    using itr = std::vector<word>::iterator;
    using citr = std::vector<word>::const_iterator;
    static constexpr word one = 0xffffffffffffffff;
    static constexpr double temperature = 6.498019;
    static constexpr int nPatterns = 60000;
    static constexpr int nClasses = 10;
    static constexpr int nOutputsPerClass = 512;
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
    word last_mask;
    int nRemainder;
    std::vector<std::vector<int>> vBias;
    torch::Tensor tLabels;

    // data
    bool fGenerated;
    bool fInitialized;
    std::vector<std::vector<int>> vOutputs;
    double dCurrentLoss;
    std::vector<word> vValues;
    std::vector<word> vValues2; // simulation with an inverter
    std::vector<word> diff;
    std::vector<word> tmp;
    std::vector<word> w; // columnpopcount
    std::vector<word> vPositives; // resimulate
    std::vector<word> vNegatives; // resimulate
    
    // marks
    unsigned iTrav;
    std::vector<unsigned> vTrav;

    // updates
    bool fUpdate;
    std::set<int> sUpdates;

    // stats
    double durationSimulation;
    double durationPopcount;
    double durationEvaluation;
    double durationDiff;
    
    // vector computations
    void Clear(int n, itr x) const;
    void Fill(int n, itr x) const;
    void Copy(int n, itr dst, citr src, bool c) const;
    void And(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const;
    void Or(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const;
    void Xor(int n, itr dst, citr src0, citr src1, bool c) const;
    bool IsZero(int n, citr x, word mask) const;
    bool IsEq(int n, citr x, citr y, bool c, word mask) const;
    bool AtMostK(int n, citr x, int k, word mask) const;
    int Popcount(int n, citr x, word mask) const;
    void Print(int n, citr x) const;
    std::vector<std::vector<word>> ColumnPopcount(std::vector<citr> const &v, std::vector<bool> const &cs);
    std::vector<std::vector<word>> ColumnPopcount2(std::vector<citr> const &v);
    std::vector<int> BinaryToInteger(std::vector<std::vector<word>> const &rs);
    std::vector<int> BinaryToInteger2(std::vector<std::vector<word>> const &rs);
    void BinarySubtract(std::vector<std::vector<word>> &a, std::vector<std::vector<word>> const &b); // a = a - b
    int BinaryPopcount(std::vector<std::vector<word>> const &a);

    // callback
    void ActionCallback(Action const &action);

    // topology
    unsigned StartTraversal(int n = 1);
    
    // simulation
    void SimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) const;
    void Simulate();
    void Resimulate();

    // generate stimuli
    void GenerateRandomStimuli();
    void ReadStimuli(Pattern *pPat);
    void GenerateExhaustiveStimuli();

    // evaluation
    void Evaluate();
    double EvaluateChange(int id);

    // preparation
    void Initialize();

  public:
    // constructors
    DlsSimulator();
    DlsSimulator(Parameter const *pPar);
    void AssignNetwork(Ntk *pNtk_, bool fReuse);

    // interface
    void SetBias(std::vector<std::vector<int>> const &vBias_);
    std::vector<std::vector<int>> GetOutputs();
    double GetLoss();
    
    // checks
    double CheckRedundancy(int id, int idx);

    // summary
    void ResetSummary();
    summary<int> GetStatsSummary() const;
    summary<double> GetTimesSummary() const;
  };

  /* {{{ xVector computations */
  
  template <typename Ntk>
  inline void DlsSimulator<Ntk>::Clear(int n, itr x) const {
    std::fill(x, x + n, 0);
  }

  template <typename Ntk>
  inline void DlsSimulator<Ntk>::Fill(int n, itr x) const {
    std::fill(x, x + n, one);
  }
  
  template <typename Ntk>
  inline void DlsSimulator<Ntk>::Copy(int n, itr dst, citr src, bool c) const {
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
  inline void DlsSimulator<Ntk>::And(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
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
  inline void DlsSimulator<Ntk>::Or(int n, itr dst, citr src0, citr src1, bool c0, bool c1) const {
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
  inline void DlsSimulator<Ntk>::Xor(int n, itr dst, citr src0, citr src1, bool c) const {
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
  inline bool DlsSimulator<Ntk>::IsZero(int n, citr x, word mask) const {
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
  inline bool DlsSimulator<Ntk>::IsEq(int n, citr x, citr y, bool c, word mask) const {
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
  inline bool DlsSimulator<Ntk>::AtMostK(int n, citr x, int k, word mask) const {
    int count = 0;
    if(mask == one) {
      for(int i = 0; i < n; i++, x++) {
        count += __builtin_popcountll(*x);
        if(count > k) {
          return false;
        }
      }
    } else {
      for(int i = 0; i < n - 1; i++, x++) {
        count += __builtin_popcountll(*x);
        if(count > k) {
          return false;
        }
      }
      count += __builtin_popcountll(*x & mask);
      if(count > k) {
        return false;
      }
    }
    return true;
  }

  template <typename Ntk>
  inline int DlsSimulator<Ntk>::Popcount(int n, citr x, word mask) const {
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
  inline void DlsSimulator<Ntk>::Print(int n, citr x) const {
    std::cout << std::bitset<64>(*x);
    x++;
    for(int i = 1; i < n; i++, x++) {
      std::cout << std::endl << std::string(10, ' ') << std::bitset<64>(*x);
    }
  }

  template <typename Ntk>
  inline std::vector<std::vector<unsigned long long>> DlsSimulator<Ntk>::ColumnPopcount(std::vector<citr> const &v, std::vector<bool> const &cs) {
    static constexpr int N = 60000 / 64 + (60000 % 64 != 0);
    static constexpr int K = nOutputsPerClass;
    static constexpr int L = 10; // clog2(K + 1)
    int d = 0;
    std::vector<std::vector<word>> rs(L);
    for(int i = 0; i < L; i++) {
      rs[i].resize(N);
    }
    // LSB
    w.resize(N * (K / 2));
    for(int i = 0; i < N; i++)
      rs[d][i] = *(v[0] + i) ^ (cs[0]? one: 0);
    for(int j = 1; j+1 < K; j += 2) {
      for(int i = 0; i < N; i++) {
        word x = *(v[j] + i) ^ (cs[j]? one: 0);
        w[N*(j/2)+i] = (rs[d][i] & x) | ((rs[d][i] | x) & (*(v[j+1] + i) ^ (cs[j+1]? one: 0)));
      }
      for(int i = 0; i < N; i++)
        rs[d][i] = rs[d][i] ^ *(v[j] + i) ^ *(v[j+1] + i) ^ (cs[j] ^ cs[j+1]? one: 0);
    }
    if(K % 2 == 0) {
      for(int i = 0; i < N; i++)
        w[N*((K-1)/2)+i] = rs[d][i] & (*(v[K-1] +i) ^ (cs[K-1]? one: 0));
      for(int i = 0; i < N; i++)
        rs[d][i] = rs[d][i] ^ *(v[K-1] + i) ^ (cs[K-1]? one: 0);
    }
    d++;
    // reduction
    while(w.size() > N) {
      int k = w.size() / N;
      for(int i = 0; i < N; i++)
        rs[d][i] = w[i];
      for(int j = 1; j+1 < k; j += 2) {
        for(int i = 0; i < N; i++)
          w[N*(j/2)+i] = (rs[d][i] & w[N*j+i]) | ((rs[d][i] | w[N*j+i]) & w[N*(j+1)+i]);
        for(int i = 0; i < N; i++)
          rs[d][i] = rs[d][i] ^ w[N*j+i] ^ w[N*(j+1)+i];
      }
      if(k % 2 == 0) {
        for(int i = 0; i < N; i++)
          w[N*((k-1)/2)+i] = rs[d][i] & w[N*(k-1)+i];
        for(int i = 0; i < N; i++)
          rs[d][i] = rs[d][i] ^ w[N*(k-1)+i];
      }
      d++;
      w.resize(N * (k / 2));
    }
    // MSB
    for(int i = 0; i < N; i++) {
      rs[d][i] = w[i];
    }
    return rs;
  }

  template <typename Ntk>
  inline std::vector<std::vector<unsigned long long>> DlsSimulator<Ntk>::ColumnPopcount2(std::vector<citr> const &v) {
    static constexpr int N = 60000 / 64 + (60000 % 64 != 0);
    int K = int_size(v);
    // clog2(K + 1)
    int L = 0;
    for(int j = K; j > 0; j >>= 1) {
      L++;
    }
    std::vector<std::vector<word>> rs(L);
    for(int i = 0; i < L; i++) {
      rs[i].resize(N);
    }
    // LSB
    int d = 0;
    w.resize(N * (K / 2));
    for(int i = 0; i < N; i++)
      rs[d][i] = *(v[0] + i);
    for(int j = 1; j+1 < K; j += 2) {
      for(int i = 0; i < N; i++) {
        word x = *(v[j] + i);
        w[N*(j/2)+i] = (rs[d][i] & x) | ((rs[d][i] | x) & *(v[j+1] + i));
      }
      for(int i = 0; i < N; i++)
        rs[d][i] = rs[d][i] ^ *(v[j] + i) ^ *(v[j+1] + i);
    }
    if(K % 2 == 0) {
      for(int i = 0; i < N; i++)
        w[N*((K-1)/2)+i] = rs[d][i] & *(v[K-1] +i);
      for(int i = 0; i < N; i++)
        rs[d][i] = rs[d][i] ^ *(v[K-1] + i);
    }
    d++;
    // reduction
    while(w.size() > N) {
      int k = w.size() / N;
      for(int i = 0; i < N; i++)
        rs[d][i] = w[i];
      for(int j = 1; j+1 < k; j += 2) {
        for(int i = 0; i < N; i++)
          w[N*(j/2)+i] = (rs[d][i] & w[N*j+i]) | ((rs[d][i] | w[N*j+i]) & w[N*(j+1)+i]);
        for(int i = 0; i < N; i++)
          rs[d][i] = rs[d][i] ^ w[N*j+i] ^ w[N*(j+1)+i];
      }
      if(k % 2 == 0) {
        for(int i = 0; i < N; i++)
          w[N*((k-1)/2)+i] = rs[d][i] & w[N*(k-1)+i];
        for(int i = 0; i < N; i++)
          rs[d][i] = rs[d][i] ^ w[N*(k-1)+i];
      }
      d++;
      w.resize(N * (k / 2));
    }
    // MSB
    if(L > 1) {
      for(int i = 0; i < N; i++) {
        rs[d][i] = w[i];
      }
    }
    return rs;
  }

  template <typename Ntk>
  inline std::vector<int> DlsSimulator<Ntk>::BinaryToInteger(std::vector<std::vector<word>> const &rs) {
    static constexpr int N = 60000 / 64 + (60000 % 64 != 0);
    std::vector<int> res(64 * N);
    for(int i = 0; i < N; i++) {
      for(int b = 0; b < 64; b++) {
        int count = 0;
        for(int j = 0; j < rs.size(); j++) {
          count |= ((rs[j][i] >> (63 - b)) & 1) << j;
        }
        res[i*64+b] = count;
      }
    }
    return res;
  }

  template <typename Ntk>
  inline std::vector<int> DlsSimulator<Ntk>::BinaryToInteger2(std::vector<std::vector<word>> const &rs) {
    static constexpr int N = 60000 / 64 + (60000 % 64 != 0);
    std::vector<int> res(64 * N);
    for(int i = 0; i < N; i++) {
      for(int b = 0; b < 64; b++) {
        int count = 0;
        for(int j = 0; j < rs.size() - 1; j++) {
          count |= ((rs[j][i] >> (63 - b)) & 1) << j;
        }
        if((rs.back()[i] >> (63 - b)) & 1) {
          count = -count;
        }
        res[i*64+b] = count;
      }
    }
    return res;
  }

  template <typename Ntk>
  inline void DlsSimulator<Ntk>::BinarySubtract(std::vector<std::vector<word>> &a, std::vector<std::vector<word>> const &b) {
    // overwrite a = a - b
    static constexpr int N = 60000 / 64 + (60000 % 64 != 0);
    int L = a.size();
    a.resize(L + 2); // carry in and out
    a[L].resize(N, one); // first carry is 1 for subtraction
    a[L+1].resize(N);
    for(int l = 0; l < L; l++) {
      for(int i = 0; i < N; i++)
        a[L+1][i] = (a[l][i] & ~b[l][i]) | ((a[l][i] | ~b[l][i]) & a[L][i]);
      for(int i = 0; i < N; i++)
        a[l][i] = a[l][i] ^ ~b[l][i] ^ a[L][i];
      std::swap(a[L], a[L+1]);
    }
    for(int i = 0; i < N; i++)
      a[L][i] = ~a[L][i];
    a.resize(L + 1);
  }

  template <typename Ntk>
  inline int DlsSimulator<Ntk>::BinaryPopcount(std::vector<std::vector<word>> const &a) {
    static constexpr int N = 60000 / 64 + (60000 % 64 != 0);
    int L = a.size() - 1;
    assert(last_mask == 0xffffffff00000000ull);
    int count = 0;
    for(int l = 0; l < L; l++) {
      int count_ = 0;
      for(int i = 0; i < N - 1; i++)
        count_ += __builtin_popcountll(a[l][i] ^ a[L][i]);
      count_ += __builtin_popcountll((a[l][N-1] ^ a[L][N-1]) & last_mask);
      count += count_ << l;
    }
    for(int i = 0; i < N - 1; i++)
      count += __builtin_popcountll(a[L][i]);
    count += __builtin_popcountll(a[L][N-1] & last_mask);
    return count;
  }
  
  /* }}} */

  /* {{{ xCallback */
  
  template <typename Ntk>
  void DlsSimulator<Ntk>::ActionCallback(Action const &action) {
    switch(action.type) {
    case REMOVE_FANIN:
      assert(fInitialized);
      sUpdates.insert(action.id);
      break;
    case REMOVE_UNUSED:
      break;
    case REMOVE_BUFFER:
    case REMOVE_CONST:
      if(fInitialized) {
        if(sUpdates.count(action.id)) {
          sUpdates.erase(action.id);
          for(int fo: action.vFanouts) {
            sUpdates.insert(fo);
          }
        }
      }
      break;
    case ADD_FANIN:
      assert(fInitialized);
      sUpdates.insert(action.id);
      break;
    case TRIVIAL_COLLAPSE:
      break;
    case TRIVIAL_DECOMPOSE:
      if(fInitialized) {
        vValues.resize(nStimuli * pNtk->GetNumNodes());
        SimulateNode(pNtk, vValues, action.fi);
        // time of this simulation is not measured for simplicity sake
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

  /* {{{ xTopology */
  
  template <typename Ntk>
  inline unsigned DlsSimulator<Ntk>::StartTraversal(int n) {
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
  
  /* {{{ xSimulation */
  
  template <typename Ntk>
  void DlsSimulator<Ntk>::SimulateNode(Ntk *pNtk_, std::vector<word> &v, int id) const {
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
  void DlsSimulator<Ntk>::Simulate() {
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
    durationSimulation += Duration(timeStart, GetCurrentTime());
    timeStart = GetCurrentTime();
    vOutputs.clear();
    {
      std::vector<citr> v(nOutputsPerClass);
      std::vector<bool> cs(nOutputsPerClass);
      int index = 0;
      pNtk->ForEachPoDriver([&](int fi, bool c) {
        v[index] = vValues.cbegin() + fi * nStimuli;
        cs[index] = c;
        index++;
        if(index == nOutputsPerClass) {
          auto rs = ColumnPopcount(v, cs);
          vOutputs.push_back(BinaryToInteger(rs));
          index = 0;
        }
      });
    }
    durationPopcount += Duration(timeStart, GetCurrentTime());
    Evaluate();
  }
  
  template <typename Ntk>
  void DlsSimulator<Ntk>::Resimulate() {
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "resimulating" << std::endl;
    }
    std::vector<int> vUpdatedPoDrivers;
    pNtk->ForEachTfosUpdate(sUpdates, false, [&](int fo) {
      bool fUpdated = false;
      {
        itr x = vValues.end();
        bool cx = false;
        switch(pNtk->GetNodeType(fo)) {
        case AND:
          pNtk->ForEachFanin(fo, [&](int fi, bool c) {
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
            Fill(nStimuli, tmp.begin());
          } else if(x != tmp.begin()) {
            Copy(nStimuli, tmp.begin(), x, cx);
          }
          break;
        default:
          assert(0);
        }
        itr y = vValues.begin() + fo * nStimuli;
        if(!IsEq(nStimuli, y, tmp.begin(), false, last_mask)) {
          if(pNtk->IsPoDriver(fo)) {
            vPositives.resize(nStimuli * (vUpdatedPoDrivers.size() + 1));
            vNegatives.resize(nStimuli * (vUpdatedPoDrivers.size() + 1));
            And(nStimuli, vPositives.begin() + vUpdatedPoDrivers.size() * nStimuli, y, tmp.begin(), true, false);
            And(nStimuli, vNegatives.begin() + vUpdatedPoDrivers.size() * nStimuli, y, tmp.begin(), false, true);
            vUpdatedPoDrivers.push_back(fo);
          }
          Copy(nStimuli, y, tmp.begin(), false);
          fUpdated = true;
          if(nVerbose) {
            std::cout << "node " << std::setw(3) << fo << ": ";
            Print(nStimuli, vValues.begin() + fo * nStimuli);
            std::cout << std::endl;
          }
        }
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
    timeStart = GetCurrentTime();
    std::set<int> s(vUpdatedPoDrivers.begin(), vUpdatedPoDrivers.end());
    {
      std::vector<citr> inc, dec;
      int index = 0, cls = 0;
      pNtk->ForEachPoDriver([&](int fi, bool c) {
        if(s.count(fi)) {
          auto it = std::find(vUpdatedPoDrivers.begin(), vUpdatedPoDrivers.end(), fi);
          int i = std::distance(vUpdatedPoDrivers.begin(), it);
          if(c) {
            inc.push_back(vNegatives.cbegin() + i * nStimuli);
            dec.push_back(vPositives.cbegin() + i * nStimuli);
          } else {
            inc.push_back(vPositives.cbegin() + i * nStimuli);
            dec.push_back(vNegatives.cbegin() + i * nStimuli);
          }
        }
        index++;
        if(index == nOutputsPerClass) {
          if(!inc.empty()) {
            auto rs = ColumnPopcount2(inc);
            auto minus = ColumnPopcount2(dec);
            BinarySubtract(rs, minus);
            auto res = BinaryToInteger2(rs);
            for(int i = 0; i < nPatterns; i++) {
              vOutputs[cls][i] += res[i];
            }
          }
          index = 0;
          cls++;
          inc.clear();
          dec.clear();
        }
      });
    }
    durationPopcount += Duration(timeStart, GetCurrentTime());
    Evaluate();
  }

  /* }}} */

  /* {{{ xStimuli */

  template <typename Ntk>
  void DlsSimulator<Ntk>::GenerateRandomStimuli() {
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
  }

  template <typename Ntk>
  void DlsSimulator<Ntk>::ReadStimuli(Pattern *pPat) {
    nStimuli = pPat->GetNumWords();
    vValues.resize(nStimuli * pNtk->GetNumNodes());
    pNtk->ForEachPiIdx([&](int index, int id) {
      Copy(nStimuli, vValues.begin() + id * nStimuli, pPat->GetIterator(index), false);
    });
    last_mask = pPat->GetLastMask();
    nRemainder = pPat->GetNumRemainder();
    assert(pPat->HasLabel());
    tLabels = torch::tensor(pPat->GetLabels(), torch::dtype(torch::kLong));
    fGenerated = true;
  }

  template <typename Ntk>
  void DlsSimulator<Ntk>::GenerateExhaustiveStimuli() {
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

  /* {{{ Evaluation */

  template <typename Ntk>
  void DlsSimulator<Ntk>::Evaluate() {
    time_point timeStart = GetCurrentTime();
    torch::Tensor logits = torch::empty({nPatterns, nClasses}, torch::kFloat64);
    double* __restrict dst = logits.data_ptr<double>();
    for(int i = 0; i < nPatterns; i++) {
      double* row = dst + i * nClasses;
      for(int cls = 0; cls < nClasses; cls++) {
        if(vBias.empty()) {
          row[cls] = static_cast<double>(vOutputs[cls][i]) / temperature;
        } else {
          row[cls] = static_cast<double>(vOutputs[cls][i] + vBias[cls][i]) / temperature;
        }
      }
    }
    torch::Tensor loss = torch::nn::functional::cross_entropy(logits, tLabels);
    dCurrentLoss = loss.item<double>();
    // auto preds = logits.argmax(1);
    // auto correct = preds.eq(tLabels).sum().item<int64_t>();
    // double accuracy = static_cast<double>(correct) / tLabels.size(0);
    // std::cout << "accuracy: " << accuracy << std::endl;
    durationEvaluation += Duration(timeStart, GetCurrentTime());
  }

  template <typename Ntk>
  double DlsSimulator<Ntk>::EvaluateChange(int id) {
    // vValues2[id] should have been set
    int target = id;
    time_point timeStart = GetCurrentTime();
    if(nVerbose) {
      std::cout << "computing loss for " << target << std::endl;
    }
    StartTraversal();
    vTrav[target] = iTrav;
    pNtk->ForEachTfo(target, false, [&](int id) {
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
    durationSimulation += Duration(timeStart, GetCurrentTime());
    timeStart = GetCurrentTime();
    std::vector<std::vector<int>> vSums;
    {
      std::vector<citr> v(nOutputsPerClass);
      std::vector<bool> cs(nOutputsPerClass);
      int index = 0;
      pNtk->ForEachPoDriver([&](int fi, bool c) {
        if(vTrav[fi] == iTrav) {
          v[index] = vValues2.cbegin() + fi * nStimuli;
        } else {
          v[index] = vValues.cbegin() + fi * nStimuli;
        }
        cs[index] = c;
        index++;
        if(index == nOutputsPerClass) {
          auto rs = ColumnPopcount(v, cs);
          vSums.push_back(BinaryToInteger(rs));
          index = 0;
        }
      });
    }
    // TODO: this could be also take diff like resim
    durationPopcount += Duration(timeStart, GetCurrentTime());
    timeStart = GetCurrentTime();
    torch::Tensor logits = torch::empty({nPatterns, nClasses}, torch::kFloat64);
    double* __restrict dst = logits.data_ptr<double>();
    for(int i = 0; i < nPatterns; i++) {
      double* row = dst + i * nClasses;
      for(int cls = 0; cls < nClasses; cls++) {
        row[cls] = static_cast<double>(vSums[cls][i] + vBias[cls][i]) / temperature;
      }
    }
    torch::Tensor loss = torch::nn::functional::cross_entropy(logits, tLabels);
    durationEvaluation += Duration(timeStart, GetCurrentTime());
    return loss.item<double>();
  }

  /* }}} */

  /* {{{ xPreparation */

  template <typename Ntk>
  void DlsSimulator<Ntk>::Initialize() {
    if(!fGenerated) {
      vValues.resize(nStimuli * pNtk->GetNumNodes());
      GenerateRandomStimuli();
      fGenerated = true;
    } else {
      vValues.resize(nStimuli * pNtk->GetNumNodes());
    }
    Simulate();
    fInitialized = true;
  }

  /* }}} */
  
  /* {{{ xConstructors */
  
  template <typename Ntk>
  DlsSimulator<Ntk>::DlsSimulator() :
    pNtk(NULL),
    nVerbose(0),
    nWords(0),
    nStimuli(0),
    last_mask(one),
    nRemainder(0),
    fGenerated(false),
    fInitialized(false),
    iTrav(0),
    fUpdate(false) {
    ResetSummary();
  }
  
  template <typename Ntk>
  DlsSimulator<Ntk>::DlsSimulator(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nSimulatorVerbose),
    nWords(pPar->nWords),
    nStimuli(nWords),
    last_mask(one),
    nRemainder(0),
    fGenerated(false),
    fInitialized(false),
    iTrav(0),
    fUpdate(false) {
    diff.resize(nWords);
    tmp.resize(nWords);
    ResetSummary();
  }

  template <typename Ntk>
  void DlsSimulator<Ntk>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    if(!fReuse) {
      fGenerated = false;
    }
    fInitialized = false;
    fUpdate = false;
    sUpdates.clear();
    pNtk = pNtk_;
    pNtk->AddCallback(std::bind(&DlsSimulator<Ntk>::ActionCallback, this, std::placeholders::_1));
    Pattern *pPat = pNtk->GetPattern();
    if(!fGenerated && pPat) {
      ReadStimuli(pPat);
      diff.resize(nStimuli);
      tmp.resize(nStimuli);
    }
  }

  /* }}} */

  /* {{{ Interface */

  template <typename Ntk>
  void DlsSimulator<Ntk>::SetBias(std::vector<std::vector<int>> const &vBias_) {
    vBias = vBias_;
  }

  template <typename Ntk>
  std::vector<std::vector<int>>DlsSimulator<Ntk>::GetOutputs() {
    if(!fInitialized) {
      Initialize();
    }
    if(!sUpdates.empty()) {
      Resimulate();
      sUpdates.clear();
    }
    return vOutputs;
  }

  template <typename Ntk>
  double DlsSimulator<Ntk>::GetLoss() {
    if(!fInitialized) {
      Initialize();
    }
    if(!sUpdates.empty()) {
      Resimulate();
      sUpdates.clear();
    }
    return dCurrentLoss;
  }
  
  /* }}} */
  
  /* {{{ xChecks */
  
  template <typename Ntk>
  double DlsSimulator<Ntk>::CheckRedundancy(int id, int idx) {
    if(!fInitialized) {
      Initialize();
    }
    if(!sUpdates.empty()) {
      Resimulate();
      sUpdates.clear();
    }
    vValues2.resize(nStimuli * pNtk->GetNumNodes());
    switch(pNtk->GetNodeType(id)) {
    case AND: {
      itr x = vValues.end();
      itr y = vValues2.begin() + id * nStimuli;
      bool cx = false;
      pNtk->ForEachFaninIdx(id, [&](int idx2, int fi, bool c) {
        if(idx == idx2) {
          return;
        }
        if(x == vValues.end()) {
          x = vValues.begin() + fi * nStimuli;
          cx = c;
        } else {
          And(nStimuli, y, x, vValues.begin() + fi * nStimuli, cx, c);
          x = y;
          cx = false;
        }
      });
      if(x == vValues.end()) {
        Fill(nStimuli, y);
      } else if(x != y) {
        Copy(nStimuli, y, x, cx);
      }
      return EvaluateChange(id);
    }
    default:
      assert(0);
    }
    return 0;
  }

  /* }}} */
  
  /* {{{ xSummary */
  
  template <typename Ntk>
  void DlsSimulator<Ntk>::ResetSummary() {
    durationSimulation = 0;
    durationPopcount = 0;
    durationEvaluation = 0;
    durationDiff = 0;
  };
  
  template <typename Ntk>
  summary<int> DlsSimulator<Ntk>::GetStatsSummary() const {
    summary<int> v;
    return v;
  };
  
  template <typename Ntk>
  summary<double> DlsSimulator<Ntk>::GetTimesSummary() const {
    summary<double> v;
    v.emplace_back("sim simulation", durationSimulation);
    v.emplace_back("sim popcount", durationPopcount);
    v.emplace_back("sim evaluation", durationEvaluation);
    v.emplace_back("sim diff computation", durationDiff);
    return v;
  };
  
  /* }}} */
  
}
