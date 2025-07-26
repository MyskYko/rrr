#pragma once

#include "rrrUtils.h"

namespace rrr {

  int decode(std::istream &in) {
    int x = 0, i = 0;
    char ch;
    while(in.get(ch) && (ch & 0x80)) {
      x |= (ch & 0x7f) << (7 * i++);
    }
    return x | (ch << (7 * i));
  }

  void encode(std::ostream &out, int x) {
    assert(x >= 0);
    char ch;
    while(x & ~0x7f) {
      ch = (x & 0x7f) | 0x80;
      out << ch;
      x >>= 7;
    }
    ch = x;
    out << ch;
  }
  
  template <typename Ntk>
  void BinaryReader(std::string const &str, Ntk *pNtk) {
    assert(pNtk->GetConst0() == 0);
    std::stringstream in(str);
    int nPis = decode(in);
    int nPos = decode(in);
    int nInts = decode(in);
    for(int i = 0; i < nPis; i++) {
      pNtk->AddPi();
    }
    for(int id = nPis + 1; id < nPis + 1 + nInts; id++) {
      int nFanins = decode(in);
      std::vector<int> vFanins(nFanins);
      std::vector<bool> vCompls(nFanins);
      /* simple
      for(int idx = 0; idx < nFanins; idx++) {
        int fi_edge = decode(in);
        int fi = fi_edge >> 1;
        bool c = fi_edge & 1;
        vFanins[idx] = fi;
        vCompls[idx] = c;
      }
      */
      int base = id << 1;
      for(int idx = nFanins - 1; idx >= 0; idx--) {
        int fi_edge_diff = decode(in);
        int fi_edge = base - fi_edge_diff;
        int fi = fi_edge >> 1;
        bool c = fi_edge & 1;
        vFanins[idx] = fi;
        vCompls[idx] = c;
        base = fi_edge;
      }
      pNtk->AddAnd(vFanins, vCompls);
    }
    for(int i = 0; i < nPos; i++) {
      int fi_edge = decode(in);
      int fi = fi_edge >> 1;
      bool c = fi_edge & 1;
      pNtk->AddPo(fi, c);
    }
  }

  template <typename Ntk>
  std::string CreateBinary(Ntk *pNtk, bool fSort = false) {
    // Assumption: POs must be located after internals
    std::stringstream ss;
    encode(ss, pNtk->GetNumPis());
    encode(ss, pNtk->GetNumPos());
    encode(ss, pNtk->GetNumInts());
    pNtk->ForEachInt([&](int id) {
      encode(ss, pNtk->GetNumFanins(id));
      /* simple
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        int fi_edge = (fi << 1) + (int)c;
        encode(ss, fi_edge);
      });
      */
      if(fSort) {
        pNtk->SortFanins(id, [&](int i, bool ci, int j, bool cj) {
          (void)ci;
          return i < j || (i == j && cj);
        });
      }
      int base = id << 1;
      pNtk->ForEachFaninReverse(id, [&](int fi, bool c) {
        int fi_edge = (fi << 1) + (int)c;
        int fi_edge_diff = base - fi_edge;
        encode(ss, fi_edge_diff);
        base = fi_edge;
      });
    });
    pNtk->ForEachPoDriver([&](int fi, bool c) {
      int fi_edge = (fi << 1) | (int)c;
      encode(ss, fi_edge);
    });
    return ss.str(); 
  }

}
