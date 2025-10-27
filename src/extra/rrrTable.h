#pragma once

#include "misc/rrrTypes.h"
#include "misc/rrrUtils.h"

namespace rrr {

  template <typename His>
  class Table {
  protected:
    float resize_factor = 0.75;
    std::vector<std::string> data;
    std::vector<int> table;
    std::vector<int> next;
    std::vector<std::vector<His>> record;

    unsigned mask;
    std::hash<std::string> hash_fn;
    unsigned hash(std::string const &str) const {
      return hash_fn(str) & mask;
    }
    /*
    unsigned hash(std::string const &str) const {
      std::hash
      static unsigned primes[16] = {31, 73, 151, 313, 643, 1291, 2593, 5233, 10501, 21013, 42073, 84181, 168451, 337219, 674701, 1349473};
      unsigned key = 0;
      for(unsigned i = 0; i < str.size(); i++) {
        key ^= primes[i&15] * str[i] * str[i];
      }
      return key & mask;
    }
    */

    void Resize() {
      unsigned size = table.size() << 1;
      assert(size);
      table.assign(size, -1);
      std::fill(next.begin(), next.end(), -1);
      mask = size - 1;
      for(int i = 0; i < int_size(data); i++) {
        unsigned h = hash(data[i]);
        next[i] = table[h];
        table[h] = i;
      }
    }
    
  public:
    Table(int size_pow) {
      unsigned size = 1 << size_pow;
      assert(size);
      table.resize(size, -1);
      data.reserve(size);
      next.reserve(size);
      record.reserve(size);
      mask = size - 1;
    }
    
    virtual ~Table() = default;

    virtual bool Register(std::string const &str, His const &his, int &index, std::string const &str2) {
      std::vector<int>::iterator it;
      for(it = table.begin() + hash(str); *it != -1; it = next.begin() + *it) {
        if(data[*it] == str) {
          record[*it].push_back(his);
          index = *it;
          return false;
        }
      }
      if(!str2.empty()) {
        std::vector<int>::iterator it2;
        for(it2 = table.begin() + hash(str2); *it2 != -1; it2 = next.begin() + *it2) {
          if(data[*it2] == str2) {
            record[*it2].push_back(his);
            index = *it2;
            return false;
          }
        }
      }
      *it = int_size(data);
      record.resize(record.size() + 1);
      record[*it].push_back(his);
      index = *it;
      data.push_back(str);
      next.push_back(-1);
      if(data.size() >= table.size() * resize_factor) {
        Resize();
      }
      return true;
    }

    std::string const &Get(int index) {
      return data[index];
    }

    int Size() {
      return int_size(data);
    }

    virtual void Deref(int index) {
      (void)index;
    }

    virtual int GetPopulation() {
      return int_size(data);
    }
    
  };
  
}
