#pragma once

#include "extra/rrrTable.h"

namespace rrr {

  template <typename His>
  class EvictTable : public Table<His> {
  private:
    using Table<His>::resize_factor;
    using Table<His>::table;
    using Table<His>::next;
    using Table<His>::data;
    using Table<His>::record;
    using Table<His>::hash;    
    using Table<His>::Resize;
    unsigned max_size;
    std::vector<bool> ref;
    
  public:
    EvictTable(int size_pow, int max_pow) :
      Table<His>(size_pow) {
      max_size = 1 << max_pow;
    }

    bool Register(std::string const &str, His const &his, int &index, std::string const &str2) override {
      std::vector<int>::iterator it;
      std::vector<int>::iterator last_unused = table.end();
      unsigned h = hash(str);
      for(it = table.begin() + h; *it != -1; it = next.begin() + *it) {
        if(data[*it] == str) {
          record[*it].push_back(his);
          index = *it;
          *it = next[index];
          next[index] = table[h];
          table[h] = index;
          return false;
        }
        if(!ref[*it]) {
          last_unused = it;
        }
      }
      if(!str2.empty()) {
        assert(0);
      }
      if((table.size() << 1) > max_size && last_unused != table.end()) {
        it = last_unused;
        data[*it] = str;
        record[*it].clear();
        record[*it].push_back(his);
        ref[*it] = true;
        index = *it;
        *it = next[index];
        next[index] = table[h];
        table[h] = index;
      } else {
        index = int_size(data);
        next.push_back(table[h]);
        table[h] = index;
        record.resize(record.size() + 1);
        record[index].push_back(his);
        ref.resize(ref.size() + 1, true);
        data.push_back(str);
      }
      if(data.size() >= table.size() * resize_factor && (table.size() << 1) <= max_size) {
        Resize();
      }
      return true;
    };

    void Deref(int index) override {
      ref[index] = false;
    }
    
  };
  
}
