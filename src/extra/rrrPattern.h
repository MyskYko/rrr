#pragma once

#include <fstream>

#include "misc/rrrUtils.h"

namespace rrr {

  class Pattern {
  private:
    using word = unsigned long long;

    int nWords = 0;
    int nRemainder = 0;
    word last_mask = 0xffffffffffffffff;
    std::vector<std::vector<word>> data;
    std::vector<std::vector<word>> data_out;

  public:
    void Read(std::string filename, int nInputs = 1);
    void ReadOutput(std::string filename, int nOutputs = 1);
    bool HasOutput() const;
    std::vector<word>::const_iterator GetIterator(int index) const;
    std::vector<word>::const_iterator GetIteratorOutput(int index) const;
    int GetNumWords() const;
    word GetLastMask() const;
    int GetNumRemainder() const;
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
    nRemainder = nSize % 8;
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
        int k = 0;
        for(; k < nRemainder; k++) {
          f.get(c);
          data[j][i] = data[j][i] << 8;
          data[j][i] += (unsigned char)c;
        }
        for(; k < 8; k++) {
          data[j][i] = data[j][i] << 8;
          last_mask = last_mask << 8;
        }
      }
    }
    nRemainder *= 8;
  }
  
  inline void Pattern::ReadOutput(std::string filename, int nOutputs) {
    std::ifstream f(filename, std::ios::binary);
    auto start = f.tellg();
    f.seekg(0, std::ios::end);
    auto end = f.tellg();
    int nBytes = end - start;
    f.seekg(0, std::ios::beg);
    std::cout << "num bytes in the file = " << nBytes << std::endl;
    assert(nBytes % nOutputs == 0);
    std::cout << "num patterns in the file = " << 8 * nBytes / nOutputs << std::endl;
    int nSize = nBytes / nOutputs;
    assert(nWords == nSize / 8 + (nSize % 8 != 0));
    data_out.resize(nOutputs);
    char c;
    nRemainder = nSize % 8;
    for(int j = 0; j < nOutputs; j++) {
      data_out[j].resize(nWords);
      int i = 0;
      for(; i < nSize / 8; i++) {
        for(int k = 0; k < 8; k++) { // 8 bytes
          f.get(c);
          data_out[j][i] = data_out[j][i] << 8;
          data_out[j][i] += (unsigned char)c;
        }
      }
      if(nSize % 8) {
        int k = 0;
        for(; k < nRemainder; k++) {
          f.get(c);
          data_out[j][i] = data_out[j][i] << 8;
          data_out[j][i] += (unsigned char)c;
        }
        for(; k < 8; k++) {
          data_out[j][i] = data_out[j][i] << 8;
        }
      }
    }
    nRemainder *= 8;
  }

  inline bool Pattern::HasOutput() const {
    return !data_out.empty();
  }

  inline std::vector<unsigned long long>::const_iterator Pattern::GetIterator(int index) const {
    return data[index].cbegin();
  }

  inline std::vector<unsigned long long>::const_iterator Pattern::GetIteratorOutput(int index) const {
    return data_out[index].cbegin();
  }

  inline int Pattern::GetNumWords() const {
    return nWords;
  }

  inline unsigned long long Pattern::GetLastMask() const {
    return last_mask;
  }

  inline int Pattern::GetNumRemainder() const {
    return nRemainder;
  }
  
}
