#pragma once

#include <iostream>
#include <vector>


namespace rrr {

  enum NodeType {
    PI,
    PO,
    AND,
    XOR,
    LUT
  };

  enum VarValue: char {
    UNDEF,
    TRUE,
    FALSE,
    TEMP_TRUE,
    TEMP_FALSE
  };

  static inline VarValue DecideVarValue(VarValue x) {
    switch(x) {
    case UNDEF:
      assert(0);
    case TRUE:
      return TRUE;
    case FALSE:
      return FALSE;
    case TEMP_TRUE:
      return TRUE;
    case TEMP_FALSE:
      return FALSE;
    default:
      assert(0);
    }
  }

  static inline char GetVarValueChar(VarValue x) {
    switch(x) {
    case UNDEF:
      return 'x';
    case TRUE:
      return '1';
    case FALSE:
      return '0';
    case TEMP_TRUE:
      return 't';
    case TEMP_FALSE:
      return 'f';
    default:
      assert(0);
    }
  }

  enum ActionType {
    NONE,
    REMOVE_FANIN,
    REMOVE_UNUSED,
    REMOVE_BUFFER,
    REMOVE_CONST,
    ADD_FANIN,
    TRIVIAL_COLLAPSE,
    TRIVIAL_DECOMPOSE,
    SAVE,
    LOAD,
    POP_BACK
  };

  struct Action {
    ActionType type = NONE;
    int id = -1;
    int idx = -1;
    int fi = -1;
    bool c;
    std::vector<int> vFanins;
    std::vector<int> vFanouts;
  };

  void PrintAction(Action action) {
    switch(action.type) {
    case REMOVE_FANIN:
      std::cout << "remove fanin " << action.id << std::endl;
      break;
    case REMOVE_UNUSED:
      std::cout << "remove unused " << action.id << std::endl;
      break;
    case REMOVE_BUFFER:
      std::cout << "remove buffer " << action.id << std::endl;
      break;
    case REMOVE_CONST:
      std::cout << "propagate " << action.id << std::endl;
      break;
    case ADD_FANIN:
      std::cout << "add fanin " << action.id << std::endl;
      break;
    default:
      assert(0);
    }
  }
  
}
