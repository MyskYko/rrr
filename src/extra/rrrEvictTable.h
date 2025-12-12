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
    int pivot;
    std::vector<bool> ref;
    
  public:
    EvictTable(int size_pow, int max_pow) :
      Table<His>(size_pow) {
      max_size = 1 << max_pow;
      pivot = 0;
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
      if((table.size() << 1) > max_size) {
        if(last_unused == table.end()) {
          int i = pivot;
          while(ref[i]) {
            i++;
            if(i == int_size(data)) {
              i = 0;
            }
            if(i == pivot) {
              break;
            }
          }
          if(ref[i]) {
            index = int_size(data);
            data.resize(data.size() + 1);
            next.resize(next.size() + 1);
            record.resize(record.size() + 1);
            ref.resize(ref.size() + 1);
          } else {
            index = i;
            record[index].clear();
            for(it = table.begin() + hash(data[index]); *it != -1; it = next.begin() + *it) {
              if(*it == index) {
                last_unused = it;
                break;
              }
            }
            assert(last_unused != table.end());
            *last_unused = next[index];
            i++;
            if(i == int_size(data)) {
              i = 0;
            }
            pivot = i;
          }
        } else {
          index = *last_unused;
          record[index].clear();
          *last_unused = next[index];
        }
      } else {
        index = int_size(data);
        data.resize(data.size() + 1);
        next.resize(next.size() + 1);
        record.resize(record.size() + 1);
        ref.resize(ref.size() + 1);
      }
      data[index] = str;
      record[index].push_back(his);
      ref[index] = true;
      next[index] = table[h];
      table[h] = index;
      if(data.size() >= table.size() * resize_factor && (table.size() << 1) <= max_size) {
        Resize();
      }
      return true;
    };

    void Deref(int index) override {
      ref[index] = false;
    }

    int GetPopulation() override {
      return std::count(ref.begin(), ref.end(), true);
    }
    
  };
  
}
