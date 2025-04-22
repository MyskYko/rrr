#pragma once

namespace rrr {
  
  template <typename Ntk>
  class Canonicalizer {
  private:
    // aliases
    static constexpr int ISO_MASK = 0xFF;
    static constexpr unsigned s_256Primes[ISO_MASK+1] = {
      0x984b6ad9,0x18a6eed3,0x950353e2,0x6222f6eb,0xdfbedd47,0xef0f9023,0xac932a26,0x590eaf55,
      0x97d0a034,0xdc36cd2e,0x22736b37,0xdc9066b0,0x2eb2f98b,0x5d9c7baf,0x85747c9e,0x8aca1055,
      0x50d66b74,0x2f01ae9e,0xa1a80123,0x3e1ce2dc,0xebedbc57,0x4e68bc34,0x855ee0cf,0x17275120,
      0x2ae7f2df,0xf71039eb,0x7c283eec,0x70cd1137,0x7cf651f3,0xa87bfa7a,0x14d87f02,0xe82e197d,
      0x8d8a5ebe,0x1e6a15dc,0x197d49db,0x5bab9c89,0x4b55dea7,0x55dede49,0x9a6a8080,0xe5e51035,
      0xe148d658,0x8a17eb3b,0xe22e4b38,0xe5be2a9a,0xbe938cbb,0x3b981069,0x7f9c0c8e,0xf756df10,
      0x8fa783f7,0x252062ce,0x3dc46b4b,0xf70f6432,0x3f378276,0x44b137a1,0x2bf74b77,0x04892ed6,
      0xfd318de1,0xd58c235e,0x94c6d25b,0x7aa5f218,0x35c9e921,0x5732fbbb,0x06026481,0xf584a44f,
      0x946e1b5f,0x8463d5b2,0x4ebca7b2,0x54887b15,0x08d1e804,0x5b22067d,0x794580f6,0xb351ea43,
      0xbce555b9,0x19ae2194,0xd32f1396,0x6fc1a7f1,0x1fd8a867,0x3a89fdb0,0xea49c61c,0x25f8a879,
      0xde1e6437,0x7c74afca,0x8ba63e50,0xb1572074,0xe4655092,0xdb6f8b1c,0xc2955f3c,0x327f85ba,
      0x60a17021,0x95bd261d,0xdea94f28,0x04528b65,0xbe0109cc,0x26dd5688,0x6ab2729d,0xc4f029ce,
      0xacf7a0be,0x4c912f55,0x34c06e65,0x4fbb938e,0x1533fb5f,0x03da06bd,0x48262889,0xc2523d7d,
      0x28a71d57,0x89f9713a,0xf574c551,0x7a99deb5,0x52834d91,0x5a6f4484,0xc67ba946,0x13ae698f,
      0x3e390f34,0x34fc9593,0x894c7932,0x6cf414a3,0xdb7928ab,0x13a3b8a3,0x4b381c1d,0xa10b54cb,
      0x55359d9d,0x35a3422a,0x58d1b551,0x0fd4de20,0x199eb3f4,0x167e09e2,0x3ee6a956,0x5371a7fa,
      0xd424efda,0x74f521c5,0xcb899ff6,0x4a42e4f4,0x747917b6,0x4b08df0b,0x090c7a39,0x11e909e4,
      0x258e2e32,0xd9fad92d,0x48fe5f69,0x0545cde6,0x55937b37,0x9b4ae4e4,0x1332b40e,0xc3792351,
      0xaff982ef,0x4dba132a,0x38b81ef1,0x28e641bf,0x227208c1,0xec4bbe37,0xc4e1821c,0x512c9d09,
      0xdaef1257,0xb63e7784,0x043e04d7,0x9c2cea47,0x45a0e59a,0x281315ca,0x849f0aac,0xa4071ed3,
      0x0ef707b3,0xfe8dac02,0x12173864,0x471f6d46,0x24a53c0a,0x35ab9265,0xbbf77406,0xa2144e79,
      0xb39a884a,0x0baf5b6d,0xcccee3dd,0x12c77584,0x2907325b,0xfd1adcd2,0xd16ee972,0x345ad6c1,
      0x315ebe66,0xc7ad2b8d,0x99e82c8d,0xe52da8c8,0xba50f1d3,0x66689cd8,0x2e8e9138,0x43e15e74,
      0xf1ced14d,0x188ec52a,0xe0ef3cbb,0xa958aedc,0x4107a1bc,0x5a9e7a3e,0x3bde939f,0xb5b28d5a,
      0x596fe848,0xe85ad00c,0x0b6b3aae,0x44503086,0x25b5695c,0xc0c31dcd,0x5ee617f0,0x74d40c3a,
      0xd2cb2b9f,0x1e19f5fa,0x81e24faf,0xa01ed68f,0xcee172fc,0x7fdf2e4d,0x002f4774,0x664f82dd,
      0xc569c39a,0xa2d4dcbe,0xaadea306,0xa4c947bf,0xa413e4e3,0x81fb5486,0x8a404970,0x752c980c,
      0x98d1d881,0x5c932c1e,0xeee65dfb,0x37592cdd,0x0fd4e65b,0xad1d383f,0x62a1452f,0x8872f68d,
      0xb58c919b,0x345c8ee3,0xb583a6d6,0x43d72cb3,0x77aaa0aa,0xeb508242,0xf2db64f8,0x86294328,
      0x82211731,0x1239a9d5,0x673ba5de,0xaf4af007,0x44203b19,0x2399d955,0xa175cd12,0x595928a7,
      0x6918928b,0xde3126bb,0x6c99835c,0x63ba1fa2,0xdebbdff0,0x3d02e541,0xd6f7aac6,0xe80b4cd0,
      0xd0fa29f1,0x804cac5e,0x2c226798,0x462f624c,0xad05b377,0x22924fcd,0xfbea205c,0x1b47586d
    };
    
    // parameters
    Ntk *            pNtk;
    /* int              nObjs; */
    /* int              nUniques; */
    /* int              nSingles; */
    /* int              nEntries; */
    
    // data
    int nMaxLevel;
    std::vector<int> vLevels;
    int nUniques;
    std::vector<int> vUniques;
    std::vector<int> vClasses, vClasses2;
    std::vector<std::pair<unsigned, int>> vStore;
    int nSim;
    std::vector<unsigned> vValues;
    std::vector<int> vOld2New;
    
    // storage util
    unsigned  GetValue(int i) const       { return vStore[i].first;  }
    int       GetItem (int i) const       { return vStore[i].second; }
    void      SetValue(int i, unsigned v) { vStore[i].first  = v;    }
    void      SetItem (int i, int id)     { vStore[i].second = id;   }

    // level
    void ComputeLevel();

    // value
    unsigned GenValue_(int v, int c) const { return (v + 1) * s_256Primes[((v << 1) + c) & ISO_MASK]; }
    unsigned GenValue(int fi, bool c) const;

    // simulation
    void Simulate();
    void SimulateBack();

    // tie break
    void AssignOneClass();

    // classfication
    void InitializeClass();
    bool Sort();
    bool Classify(bool fForwardFirst);

    // construct
    int ConstructRec(Ntk *pNew, int id);
    
  public:
    //constructor
    Canonicalizer() {};

    // run
    void Run(Ntk *pNtk_);
  };

  /* {{{ Level */

  template <typename Ntk>
  void Canonicalizer<Ntk>::ComputeLevel() {
    vLevels.clear();
    vLevels.resize(pNtk->GetNumNodes());
    pNtk->ForEachInt([&](int id) {
      pNtk->ForEachFanin(id, [&](int fi) {
        if(vLevels[id] < vLevels[fi]) {
          vLevels[id] = vLevels[fi];
        }
      });
      vLevels[id] += 1;
    });
    nMaxLevel = 0;
    pNtk->ForEachPo([&](int id) {
      int fi = pNtk->GetFanin(id, 0);
      vLevels[id] = vLevels[fi] + 1;
      if(nMaxLevel < vLevels[id]) {
        nMaxLevel = vLevels[id];
      }
    });
  }

  /* }}} */

  /* {{{ Value */
  
  template <typename Ntk>
  unsigned Canonicalizer<Ntk>::GenValue(int fi, bool c) const {
    if(nSim == 0) {
      return GenValue_(vLevels[fi] + nMaxLevel * pNtk->GetNumFanins(fi), c);
    }
    if(vUniques[fi]) {
      return GenValue_(vUniques[fi], c);
    }
    return 0;
  }

  /* }}} */  

  /* {{{ Simulation */
  
  template <typename Ntk>
  void Canonicalizer<Ntk>::Simulate() {
    // initialize constant and PIs
    vValues[pNtk->GetConst0()] += s_256Primes[ISO_MASK];
    pNtk->ForEachPiIdx([&](int idx, int id) {
      vValues[id] += s_256Primes[(ISO_MASK - idx - 1) & ISO_MASK];
    });
    // simulate nodes
    pNtk->ForEachInt([&](int id) {
      pNtk->ForEachFanin(id, [&](int fi, bool c){
        vValues[id] += vValues[fi] + GenValue(fi, c);
      });
    });
    // simulate POs
    pNtk->ForEachPo([&](int id) {
      int fi = pNtk->GetFanin(id, 0);
      bool c = pNtk->GetCompl(id, 0);
      vValues[id] += vValues[fi] + GenValue(fi, c);
    });
    nSim++;
  }

  template <typename Ntk>
  void Canonicalizer<Ntk>::SimulateBack() {
    // simulate POs
    pNtk->ForEachPo([&](int id) {
      int fi = pNtk->GetFanin(id, 0);
      bool c = pNtk->GetCompl(id, 0);
      vValues[fi] += vValues[id] + GenValue(id, c);
    });
    // simulate nodes
    pNtk->ForEachInt([&](int id) {
      pNtk->ForEachFanin(id, [&](int fi, bool c){
        vValues[fi] += vValues[id] + GenValue(id, c);
      });
    });
    nSim++;
  }

  /* }}} */

  /* {{{ Tie break */

  template <typename Ntk>
  void Canonicalizer<Ntk>::AssignOneClass() {
    // find the classes with the highest level and same number of fanins
    assert(!vClasses.empty());
    int iBegin0 = vClasses[vClasses.size() - 2];
    int i;
    for(i = int_size(vClasses) - 4; i >= 0; i -= 2) {
      int iBegin = vClasses[i];
      if(vLevels[GetItem(iBegin)] != vLevels[GetItem(iBegin0)]) {
        break;
      }
      if(pNtk->GetNumFanins(GetItem(iBegin)) != pNtk->GetNumFanins(GetItem(iBegin0))) {
        break;
      }
    }
    i += 2;
    assert( i >= 0 );
    // assign all classes starting with this one
    int Shrink = i;
    for(; i < int_size(vClasses); i += 2) {
      int iBegin = vClasses[i];
      int nSize  = vClasses[i + 1];
      for(int j = 0; j < nSize; j++) {
        vUniques[GetItem(iBegin + j)] = nUniques++;
      }
    }
    vClasses.resize(Shrink);
  }
  
  /* }}} */
  
  /* {{{ Classification */
  
  template <typename Ntk>
  void Canonicalizer<Ntk>::InitializeClass() {
    // classify with level and number of fanins
    std::vector<std::vector<std::vector<int>>> nodes(nMaxLevel + 1);
    pNtk->ForEachInt([&](int id) {
      int nFanins = pNtk->GetNumFanins(id);
      assert(nFanins > 1);
      if(int_size(nodes[vLevels[id]]) <= nFanins) {
        nodes[vLevels[id]].resize(nFanins + 1);
      }
      nodes[vLevels[id]][nFanins].push_back(id);
    });
    // register
    int nItems = 0;
    vClasses.clear();
    for(std::vector<std::vector<int>> const &vv: nodes) {
      for(std::vector<int> const &v: vv) {
        if(!v.empty()) {
          if(v.size() == 1) {
            vUniques[v[0]] = nUniques++;
          } else {
            vClasses.push_back(nItems);
            vClasses.push_back(int_size(v));
            for(int id: v) {
              SetItem(nItems++, id);
            }
          }
        }
      }
    }
    // print the results
    /*
    for(int i = 0; i < int_size(vClasses) / 2; i++) {
      std::cout << "class " << i << "(" << vClasses[i + i] << ", " << vClasses[i + i + 1] << "):" << std::endl << "\t";
      for(int j = 0; j < vClasses[i + i + 1]; j++) {
        int id = GetItem(vClasses[i + i] + j);
        std::cout << id << ", ";
      }
      std::cout << std::endl;
    }
    std::cout << "uniques: ";
    pNtk->ForEachInt([&](int id) {
      if(vUniques[id]) {
        std::cout << id << ", ";
      }
    });
    std::cout << std::endl;
    */
  }

  template <typename Ntk>
  bool Canonicalizer<Ntk>::Sort() {
    bool fRefined = false;
    vClasses2.clear();
    for(int i = 0; i < int_size(vClasses) / 2; i++) {
      int iBegin = vClasses[i + i];
      int nSize = vClasses[i + i + 1];
      {
        // transfer values
        bool fSameValue = true;
        unsigned v0 = vValues[GetItem(iBegin)];
        for(int j = 0; j < nSize; j++) {
          unsigned v = vValues[GetItem(iBegin + j)];
          SetValue(iBegin + j, v);
          if(v0 != v) {
            fSameValue = false;
          }
        }
        // if no distinct values
        if(fSameValue) {
          vClasses2.push_back(iBegin);
          vClasses2.push_back(nSize);
          continue;
        }
      }
      fRefined = true;
      // sort
      /*
      std::cout << "from: ";
      for(auto it = vStore.begin() + iBegin; it != vStore.begin() + iBegin + nSize; it++) {
        std::cout << "(" << it->first << ", " << it->second << "), ";
      }
      std::cout << std::endl;
      */
      std::sort(vStore.begin() + iBegin, vStore.begin() + iBegin + nSize);
      /*
      std::cout << "to  : ";
      for(auto it = vStore.begin() + iBegin; it != vStore.begin() + iBegin + nSize; it++) {
        std::cout << "(" << it->first << ", " << it->second << "), ";
      }
      std::cout << std::endl;
      */
      // divide into new classes
      int iBeginOld = iBegin;
      unsigned v0 = vValues[GetItem(iBegin)];
      for(int j = 1; j < nSize; j++) {
        unsigned v = vValues[GetItem(iBegin + j)];
        if(v == v0) {
          continue;
        }
        int nSizeNew = iBegin + j - iBeginOld;
        if(nSizeNew == 1) {
          vUniques[GetItem(iBeginOld)] = nUniques++;
        } else {
          vClasses2.push_back(iBeginOld);
          vClasses2.push_back(nSizeNew);
        }
        iBeginOld = iBegin + j;
        v0 = v;
      }
      // add the last one
      int nSizeNew = iBegin + nSize - iBeginOld;
      if(nSizeNew == 1) {
        vUniques[GetItem(iBeginOld)] = nUniques++;
      } else {
        vClasses2.push_back(iBeginOld);
        vClasses2.push_back(nSizeNew);
      }
    }
    vClasses.swap(vClasses2);
    return fRefined;
  }
  
  template <typename Ntk>
  bool Canonicalizer<Ntk>::Classify(bool fForwardFirst) {
    int nFixedPoint = 1; // ???
    bool fRefined = false;
    if(fForwardFirst) {
      for(int c = 1; c <= nFixedPoint + 1; c++) {
        Simulate();
        if(Sort()) {
          c = 0;
          fRefined = true;
        }
      }
    }
    for(int c = 1; c <= nFixedPoint + 1; c++) {
      SimulateBack();
      if(Sort()) {
        c = 0;
        fRefined = true;
      }
    }
    if(!fForwardFirst) {
      for(int c = 1; c <= nFixedPoint + 1; c++) {
        Simulate();
        if(Sort()) {
          c = 0;
          fRefined = true;
        }
      }
    }
    return fRefined;
  }

  /* }}} */

  /* {{{ Construct */

  template <typename Ntk>
  int Canonicalizer<Ntk>::ConstructRec(Ntk *pNew, int id) {
    if(vOld2New[id] != -1) {
      return vOld2New[id];
    }
    std::vector<std::pair<int, int>> vFaninIdxs;
    pNtk->ForEachFaninIdx(id, [&](int idx, int fi) {
      vFaninIdxs.emplace_back(vUniques[fi], idx);
    });
    std::sort(vFaninIdxs.begin(), vFaninIdxs.end());
    int i = 0;
    std::vector<int> vFanins(vFaninIdxs.size());
    std::vector<bool> vCompls(vFaninIdxs.size());
    for(std::pair<int, int> const &entry: vFaninIdxs) {
      int idx = entry.second;
      int fi = pNtk->GetFanin(id, idx);
      vFanins[i] = ConstructRec(pNew, fi);
      vCompls[i] = pNtk->GetCompl(id, idx);
      i++;
    }
    vOld2New[id] = pNew->AddAnd(vFanins, vCompls);
    return vOld2New[id];
  }

  /* }}} */
  
  /* {{{ Run */
  
  template <typename Ntk>
  void Canonicalizer<Ntk>::Run(Ntk *pNtk_) {
    // network must have been fully swept and trivial-collapsed
    pNtk = pNtk_;
    // checks
    if(pNtk->GetNumPis() == 0) {
      assert(pNtk->GetNumInts() == 0);
      return;
    }
    assert(pNtk->GetNumPis() > 0);
    assert(pNtk->GetNumPos() > 0);
    // initialization
    ComputeLevel();
    nUniques = pNtk->GetNumPis() + 1;
    vUniques.clear();
    vUniques.resize(pNtk->GetNumNodes());
    vStore.resize(pNtk->GetNumNodes());
    InitializeClass();
    nSim = 0;
    vValues.clear();
    vValues.resize(pNtk->GetNumNodes());
    // classification
    while(!vClasses.empty() && Classify(true)) {
    }
    while(!vClasses.empty()) {
      if(!Classify(false)) {
        // tie break
        AssignOneClass();
      }
    }
    // construct canonicalized network
    Ntk ntk;
    vOld2New.clear();
    vOld2New.resize(pNtk->GetNumNodes(), -1);
    vOld2New[pNtk->GetConst0()] = ntk.GetConst0();
    pNtk->ForEachPiIdx([&](int idx, int id) {
      vOld2New[id] = ntk.AddPi();
      vUniques[id] = idx + 1;
    });
    std::vector<std::pair<int, bool>> vPoDrivers;
    pNtk->ForEachPoDriver([&](int id, bool c) {
      int fi = ConstructRec(&ntk, id);
      vPoDrivers.emplace_back(fi, c);
    });
    for(std::pair<int, bool> const &entry: vPoDrivers) {
      ntk.AddPo(entry.first, entry.second);
    }
    // read
    pNtk->Read(ntk);
  }

  /* }}} */
  
}
