#pragma once

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
  void AigReader(std::string const &str, Ntk *pNtk) {
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
    assert(token == "0"); // no latches
    std::getline(ss, token, ' ');
    int nPos = std::stoi(token);
    std::getline(ss, token, ' ');
    int nInts = stoi(token);
    assert(nObjs == nInts + nPis);
    nObjs++; // constant
    // contents
    pNtk->Reserve(nObjs);
    for(int i = 0; i < nPis; i++) {
      pNtk->AddPi();
    }
    std::vector<int> vPos(nPos);
    for(int i = 0; i < nPos; i++) {
      std::getline(f, token);
      vPos[i] = stoi(token);
    }
    for(int i = nPis + 1; i < nObjs; i++) {
      int n0 = i + i - decode(f);
      int n1 = n0 - decode(f);
      pNtk->AddAnd(n0 >> 1, n1 >> 1, n0 & 1, n1 & 1);
    }
    for(int i = 0; i < nPos; i++) {
      pNtk->AddPo(vPos[i] >> 1, vPos[i] & 1);
    }
  }

  template <typename Ntk>
  std::string CreateAig(Ntk *pNtk) {
    std::stringstream ss;
    ss << "aig " << pNtk->GetNumPis() + pNtk->GetNumInts() << " " << pNtk->GetNumPis() << " 0 " << pNtk->GetNumPos() << " " << pNtk->GetNumInts() << std::endl;
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
    pNtk->ForEachPoDriver([&](int fi, bool c) {
      ss << (vValues[fi] ^ (int)c) << std::endl;
    });
    pNtk->ForEachInt([&](int id) {
      if(pNtk->GetNumFanins(id) > 1) {
        int n0 = vValues[pNtk->GetFanin(id, 0)] ^ (int)pNtk->GetCompl(id, 0);
        int n1 = vValues[pNtk->GetFanin(id, 1)] ^ (int)pNtk->GetCompl(id, 1);
        if(n0 < n1) {
          std::swap(n0, n1);
        }
        encode(ss, vValues[id] - n0);
        encode(ss, n0 - n1);
        for(int i = 2; i < pNtk->GetNumFanins(id); i++) {
          encode(ss, 2);
          encode(ss, vValues[id] - (vValues[pNtk->GetFanin(id, i)] ^ (int)pNtk->GetCompl(id, i)));
          vValues[id] += 2;
        }
      }
    });
    return ss.str();
  }

  template <typename Ntk>
  void AigFileReader(std::string const &filename, Ntk *pNtk) {
    std::stringstream ss;
    std::ifstream f(filename, std::ios_base::binary);
    ss << f.rdbuf();
    std::string str = ss.str();
    AigReader(str, pNtk);
  }

  template <typename Ntk>
  void DumpAig(std::string filename, Ntk *pNtk) {
    std::string str = CreateAig(pNtk);
    std::ofstream f(filename, std::ios_base::binary);
    f << str;
  }

}
