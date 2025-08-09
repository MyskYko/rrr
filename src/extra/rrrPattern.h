#pragma once

#include <fstream>

#include "misc/rrrUtils.h"

namespace rrr {

  class Pattern {
  private:
    using word = unsigned long long;

    int nWords;
    std::vector<std::vector<word>> data;

  public:
    void Read(std::string filename, int nInputs = 1);
    std::vector<word>::const_iterator GetIterator(int index) const;
    int GetNumWords() const;
  };

  inline void Pattern::Read(std::string filename, int nInputs) {
    std::ifstream f(filename, std::ios::binary);
    auto start = f.tellg();
    f.seekg(0, std::ios::end);
    auto end = f.tellg();
    int nBytes = end - start;
    f.seekg(0, std::ios::beg);
    std::cout << "num bytes in the file = " << nBytes << std::endl;
    assert(nBytes % nInputs == 0);
    std::cout << "num patterns in the file = " << 8 * nBytes / nInputs << std::endl;
    int nSize = nBytes / nInputs;
    nWords = nSize / 8 + (nSize % 8 != 0);
    data.resize(nInputs);
    char c;
    for(int j = 0; j < nInputs; j++) {
      data[j].resize(nWords);
      int i = 0;
      for(; i < nSize / 8; i++) {
        for(int k = 0; k < 8; k++) { // 8 bytes
          f.get(c);
          data[j][i] = data[j][i] << 8;
          data[j][i] += (unsigned char)c;
        }
      }
      if(nSize % 8) {
        int nRemainder = nSize % 8;
        int k = 0;
        for(; k < nRemainder; k++) {
          f.get(c);
          data[j][i] = data[j][i] << 8;
          data[j][i] += (unsigned char)c;
        }
        for(; k < 8; k++) {
          data[j][i] = data[j][i] << 8;
        }
      }
    }
  }

  inline std::vector<unsigned long long>::const_iterator Pattern::GetIterator(int index) const {
    return data[index].cbegin();
  }

  inline int Pattern::GetNumWords() const {
    return nWords;
  }
}
