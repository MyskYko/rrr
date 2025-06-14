#pragma once

#include <vector>
#include <chrono>

namespace rrr {

  enum NodeType {
    PI,
    PO,
    AND,
    XOR,
    LUT
  };

  enum SatResult {
    SAT,
    UNSAT,
    UNDET
  };

  enum VarValue: char {
    UNDEF,
    rrrTRUE,
    rrrFALSE,
    TEMP_TRUE,
    TEMP_FALSE
  };

  enum ActionType {
    NONE,
    REMOVE_FANIN,
    REMOVE_UNUSED,
    REMOVE_BUFFER,
    REMOVE_CONST,
    ADD_FANIN,
    TRIVIAL_COLLAPSE,
    TRIVIAL_DECOMPOSE,
    SORT_FANINS,
    READ,
    SAVE,
    LOAD,
    POP_BACK,
    INSERT
  };

  struct Action {
    ActionType type = NONE;
    int id = -1;
    int idx = -1;
    int fi = -1;
    bool c = false;
    std::vector<int> vFanins;
    std::vector<int> vIndices;
    std::vector<int> vFanouts;
  };

  using seconds = int64_t;
  using clock_type = std::chrono::steady_clock;
  using time_point = std::chrono::time_point<clock_type>;

  template <typename T>
  using summary = std::vector<std::pair<std::string, T>>;

}
