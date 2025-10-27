#pragma once

#include <vector>
#include <sstream>
#include <fstream>
#include <cassert>

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
  int AigReader(std::string const &str, Ntk *pNtk) {
    assert(pNtk->GetConst0() == 0);
    std::stringstream f(str);
    std::string line;
    std::getline(f, line);
    std::stringstream ss(line);
    std::string token;
    std::getline(ss, token, ' ');
    assert(token == "aig");
    std::getline(ss, token, ' ');
    int nObjs = std::stoi(token);
    std::getline(ss, token, ' ');
    int nPis = std::stoi(token);
    std::getline(ss, token, ' ');
    int nLatches = std::stoi(token);
    std::getline(ss, token, ' ');
    int nPos = std::stoi(token);
    std::getline(ss, token, ' ');
    int nInts = std::stoi(token);
    assert(nObjs == nInts + nPis + nLatches);
    nObjs++; // constant
    // contents
    pNtk->Reserve(nObjs);
    for(int i = 0; i < nPis; i++) {
      pNtk->AddPi();
    }
    std::vector<int> vLatches(nLatches);
    for(int i = 0; i < nLatches; i++) {
      std::getline(f, token);
      vLatches[i] = std::stoi(token);
      pNtk->AddPi();
    }
    std::vector<int> vPos(nPos);
    for(int i = 0; i < nPos; i++) {
      std::getline(f, token);
      vPos[i] = std::stoi(token);
    }
    for(int i = nPis + nLatches + 1; i < nObjs; i++) {
      int n0 = i + i - decode(f);
      int n1 = n0 - decode(f);
      pNtk->AddAnd(n1 >> 1, n0 >> 1, n1 & 1, n0 & 1);
    }
    for(int i = 0; i < nLatches; i++) {
      pNtk->AddPo(vLatches[i] >> 1, vLatches[i] & 1);
    }
    for(int i = 0; i < nPos; i++) {
      pNtk->AddPo(vPos[i] >> 1, vPos[i] & 1);
    }
    return nLatches;
  }

  template <typename Ntk>
  std::string CreateAig(Ntk *pNtk, int nLatches) {
    std::vector<int> vValues(pNtk->GetNumNodes());
    int nNodes = 0;
    vValues[pNtk->GetConst0()] = nNodes++ << 1;
    pNtk->ForEachPi([&](int id) {
      vValues[id] = nNodes++ << 1;
    });
    pNtk->ForEachInt([&](int id) {
      if(pNtk->GetNumFanins(id) == 0) { // constant 1
        vValues[id] = vValues[pNtk->GetConst0()] ^ 1;
      } else if(pNtk->GetNumFanins(id) == 1) { // buffer/inverter
        vValues[id] = vValues[pNtk->GetFanin(id, 0)] ^ (int)pNtk->GetCompl(id, 0);
      } else {
        vValues[id] = nNodes << 1;
        nNodes += pNtk->GetNumFanins(id) - 1;
      }
    });
    std::stringstream ss;
    pNtk->ForEachInt([&](int id) {
      if(pNtk->GetNumFanins(id) > 1) {
        int i = pNtk->GetNumFanins(id) - 1;
        int n0 = vValues[pNtk->GetFanin(id, i)] ^ (int)pNtk->GetCompl(id, i);
        i--;
        int n1 = vValues[pNtk->GetFanin(id, i)] ^ (int)pNtk->GetCompl(id, i);
        i--;
        if(n0 < n1) {
          std::swap(n0, n1);
        }
        encode(ss, vValues[id] - n0);
        encode(ss, n0 - n1);
        while(i >= 0) {
          encode(ss, 2);
          encode(ss, vValues[id] - (vValues[pNtk->GetFanin(id, i)] ^ (int)pNtk->GetCompl(id, i)));
          i--;
          vValues[id] += 2;
        }
      }
    });
    std::stringstream ss0;
    ss0 << "aig " << nNodes - 1 << " " << pNtk->GetNumPis() - nLatches << " " << nLatches << " " << pNtk->GetNumPos() - nLatches << " " << nNodes - pNtk->GetNumPis() - 1 << std::endl;
    pNtk->ForEachPoDriver([&](int fi, bool c) {
      ss0 << (vValues[fi] ^ (int)c) << std::endl;
    });
    return ss0.str() + ss.str();
  }

  template <typename Ntk>
  int AigFileReader(std::string const &filename, Ntk *pNtk) {
    std::stringstream ss;
    std::ifstream f(filename, std::ios_base::binary);
    ss << f.rdbuf();
    std::string str = ss.str();
    return AigReader(str, pNtk);
  }

  template <typename Ntk>
  void DumpAig(std::string filename, Ntk *pNtk, int nLatches = 0) {
    std::string str = CreateAig(pNtk, nLatches);
    std::ofstream f(filename, std::ios_base::binary);
    f << str;
  }

}
