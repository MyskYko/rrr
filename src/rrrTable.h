#pragma once

#include "rrrTypes.h"
#include "rrrUtils.h"

namespace rrr {

  class Table {
  private:
    std::vector<std::string> data;
    std::vector<int> table;
    std::vector<int> next;
    std::vector<std::vector<std::vector<int>>> record;

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

    std::vector<int> translate(int src, std::vector<Action> const &vActions) const {
      std::vector<int> v;
      v.push_back(src);
      for(auto const &action: vActions) {
        if(action.type == REMOVE_FANIN) {
          v.push_back(action.id);
          v.push_back(action.idx);
        } else if(action.type == ADD_FANIN) {
          v.push_back(-action.id);
          v.push_back(action.fi);
        }
      }
      return v;
    }
    
  public:
    Table(int size_pow) {
      unsigned size = 1 << size_pow;
      table.resize(size, -1);
      data.reserve(size);
      next.reserve(size);
      record.reserve(size);
      mask = size - 1;
    }

    bool Register(std::string const &str, int src, std::vector<Action> const &vActions, int &index, std::string const &str2) {
      std::vector<int>::iterator it;
      for(it = table.begin() + hash(str); *it != -1; it = next.begin() + *it) {
        if(data[*it] == str) {
          record[*it].push_back(translate(src, vActions));
          index = *it;
          return false;
        }
      }
      if(!str2.empty()) {
        std::vector<int>::iterator it2;
        for(it2 = table.begin() + hash(str2); *it2 != -1; it2 = next.begin() + *it2) {
          if(data[*it2] == str2) {
            record[*it2].push_back(translate(src, vActions));
            index = *it2;
            return false;
          }
        }
      }
      *it = int_size(data);
      data.push_back(str);
      next.push_back(-1);
      record.resize(record.size() + 1);
      record[*it].push_back(translate(src, vActions));
      index = *it;
      return true;
    }

    std::string const &Get(int index) {
      return data[index];
    }

    int Size() {
      return int_size(data);
    }
    
  };
  
}
