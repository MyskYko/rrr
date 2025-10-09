#pragma once

#include <fstream>

#include "misc/rrrUtils.h"

namespace rrr {

  class Pattern {
  private:
    using word = unsigned long long;

    int nWords = 0;
    int nRemainder = 0;
    word wLastMask = 0xffffffffffffffff;
    std::vector<std::vector<word>> data;
    std::vector<std::vector<word>> data_out;
    std::vector<std::vector<word>> data_other;
    std::vector<int> label;

  public:
    void Read(std::string filename, int nInputs = 1);
    void ReadOutput(std::string filename, int nOutputs = 1);
    void ReadOther(std::string filename, int nOutputs = 1);
    void ReadLabel(std::string filename);
    bool HasOutput() const;
    bool HasOther() const;
    bool HasLabel() const;
    std::vector<word>::const_iterator GetIterator(int index) const;
    std::vector<word>::const_iterator GetIteratorOutput(int index) const;
    std::vector<word>::const_iterator GetIteratorOther(int index) const;
    int GetLabel(int index) const;
    std::vector<int> GetLabels() const;
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
    wLastMask = 0xffffffffffffffff;
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
        }
      }
    }
    for(int k = 0; k < nRemainder; k++) {
      wLastMask = wLastMask << 8;
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
    int nRemainder_ = nSize % 8;
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
        for(; k < nRemainder_; k++) {
          f.get(c);
          data_out[j][i] = data_out[j][i] << 8;
          data_out[j][i] += (unsigned char)c;
        }
        for(; k < 8; k++) {
          data_out[j][i] = data_out[j][i] << 8;
        }
      }
    }
    nRemainder_ *= 8;
    assert(nRemainder == nRemainder_);
  }

  inline void Pattern::ReadOther(std::string filename, int nOutputs) {
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
    data_other.resize(nOutputs);
    char c;
    int nRemainder_ = nSize % 8;
    for(int j = 0; j < nOutputs; j++) {
      data_other[j].resize(nWords);
      int i = 0;
      for(; i < nSize / 8; i++) {
        for(int k = 0; k < 8; k++) { // 8 bytes
          f.get(c);
          data_other[j][i] = data_other[j][i] << 8;
          data_other[j][i] += (unsigned char)c;
        }
      }
      if(nSize % 8) {
        int k = 0;
        for(; k < nRemainder_; k++) {
          f.get(c);
          data_other[j][i] = data_other[j][i] << 8;
          data_other[j][i] += (unsigned char)c;
        }
        for(; k < 8; k++) {
          data_other[j][i] = data_other[j][i] << 8;
        }
      }
    }
    nRemainder_ *= 8;
    assert(nRemainder == nRemainder_);
  }

  inline void Pattern::ReadLabel(std::string filename) {
    std::ifstream f(filename);
    int nPatterns = nWords * 64;
    if(nRemainder) {
      nPatterns += nRemainder;
      nPatterns -= 64;
    }
    label.reserve(nPatterns);
    std::string str;
    while(std::getline(f, str)) {
      label.push_back(std::stoi(str));
    }
    assert(int_size(label) == nPatterns);
  }

  inline bool Pattern::HasOutput() const {
    return !data_out.empty();
  }

  inline bool Pattern::HasOther() const {
    return !data_other.empty();
  }
  
  inline bool Pattern::HasLabel() const {
    return !label.empty();
  }

  inline std::vector<unsigned long long>::const_iterator Pattern::GetIterator(int index) const {
    return data[index].cbegin();
  }

  inline std::vector<unsigned long long>::const_iterator Pattern::GetIteratorOutput(int index) const {
    return data_out[index].cbegin();
  }

  inline std::vector<unsigned long long>::const_iterator Pattern::GetIteratorOther(int index) const {
    return data_other[index].cbegin();
  }
  
  inline int Pattern::GetLabel(int index) const {
    return label[index];
  }

  inline std::vector<int> Pattern::GetLabels() const {
    return label;
  }

  inline int Pattern::GetNumWords() const {
    return nWords;
  }

  inline unsigned long long Pattern::GetLastMask() const {
    return wLastMask;
  }

  inline int Pattern::GetNumRemainder() const {
    return nRemainder;
  }
  
}
