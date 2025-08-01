#pragma once

#include <sat/bsat/satSolver.h>

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"

namespace rrr {

  template <typename Ntk>
  class SatSolver {
  private:
    // pointer to network
    Ntk *pNtk;

    // parameters
    int nVerbose;
    int nConflictLimit;

    // data
    sat_solver *pSat;
    bool status; // false indicates trivial UNSAT
    int  target; // node for which miter has been encoded
    std::vector<int> vVars; // SAT variable for each node
    std::vector<int> vLits; // temporary storage
    std::vector<VarValue> vValues; // values in satisfied problem
    bool fUpdate;

    // stats
    int nCalls;
    int nSats;
    int nUnsats;
    double durationRedundancy;
    double durationFeasibility;

    // callback
    void ActionCallback(Action const &action);

    // encode
    void EncodeNode(sat_solver *p, std::vector<int> const &v, int id, int to_negate = -1) const;
    void EncodeMiter(sat_solver *p, std::vector<int> &v, int id); // create a careset miter where the counterpart has the output of target negated
    void SetTarget(int id);
    
  public:
    // constructors
    SatSolver(Parameter const *pPar);
    ~SatSolver();
    void AssignNetwork(Ntk *pNtk_, bool fReuse);
    
    // checks
    SatResult CheckRedundancy(int id, int idx);
    SatResult CheckFeasibility(int id, int fi, bool c);

    // cex
    std::vector<VarValue> GetCex();

    // stats
    void ResetSummary();
    summary<int> GetStatsSummary() const;
    summary<double> GetTimesSummary() const;
  };

  /* {{{ Callback */

  template <typename Ntk>
  void SatSolver<Ntk>::ActionCallback(Action const &action) {
    if(target == -1) {
      return;
    }
    switch(action.type) {
    case REMOVE_FANIN:
      if(action.id != target) {
        fUpdate = true;
      }
      break;
    case REMOVE_UNUSED:
      break;
    case REMOVE_BUFFER:
    case REMOVE_CONST:
      if(action.id == target) {
        target = -1;
      }
      break;
    case ADD_FANIN:
      if(action.id != target) {
        fUpdate = true;
      }
      break;
    case TRIVIAL_COLLAPSE:
      break;
    case TRIVIAL_DECOMPOSE:
      fUpdate = true;
      break;
    case SORT_FANINS:
      break;
    case READ:
      status = false;
      target = -1;
      fUpdate = false;
      break;
    case SAVE:
      break;
    case LOAD:
      target = -1;
      break;
    case POP_BACK:
      break;
    default:
      assert(0);
    }
  }

  /* }}} */
  
  /* {{{ Encode */

  template <typename Ntk>
  void SatSolver<Ntk>::EncodeNode(sat_solver *p, std::vector<int> const &v, int id, int to_negate) const {
    int RetValue;
    int x = -1, y = -1;
    bool cx = false, cy = false;
    assert(pNtk->GetNodeType(id) == AND);
    std::string delim;
    if(nVerbose) {
      std::cout << "node " << std::setw(3) << id << ": ";
    }
    pNtk->ForEachFanin(id, [&](int fi, bool c) {
      if(x == -1) {
        x = v[fi];
        cx = c ^ (fi == to_negate);
      } else if(y == -1) {
        y = v[fi];
        cy = c ^ (fi == to_negate);
      } else {
        int z = sat_solver_addvar(p);
        if(nVerbose) {
          std::cout << delim << z << " = " << (cx? "!": "") << x << " & " << (cy? "!": "") << y << std::endl;
          delim = std::string(10, ' ');
        }
        RetValue = sat_solver_add_and(p, z, x, y, cx, cy, 0);
        assert(RetValue);
        x = z;
        cx = false;
        y = v[fi];
        cy = c ^ (fi == to_negate);
      }
    });
    if(x == -1) {
      if(nVerbose) {
        std::cout << delim << v[id] << " = !0" << std::endl;
      }
      RetValue = sat_solver_add_const(p, v[id], 0);
      assert(RetValue);
    } else if(y == -1) {
      if(nVerbose) {
        std::cout << delim << v[id] << " = " << (cx? "!": "") << x << std::endl;
      }
      RetValue = sat_solver_add_buffer(p, v[id], x, cx);
      assert(RetValue);
    } else {
      if(nVerbose) {
        std::cout << delim << v[id] << " = " << (cx? "!": "") << x << " & " << (cy? "!": "") << y << std::endl;
      }
      RetValue = sat_solver_add_and(p, v[id], x, y, cx, cy, 0);
      assert(RetValue);
    }
  }
  
  template <typename Ntk>
  void SatSolver<Ntk>::EncodeMiter(sat_solver *p, std::vector<int> &v, int id) {
    int RetValue;
    // reset
    v.clear();
    sat_solver_restart(p);
    status = true;
    // assign vars for the base
    int nNodes = pNtk->GetNumNodes();
    sat_solver_setnvars(p, nNodes);
    v.resize(nNodes);
    for(int i = 0; i < nNodes; i++) {
      v[i] = i;
    }
    // constrain const-0
    RetValue = sat_solver_add_const(p, v[0], 1);
    assert(RetValue);
    // encode first circuit
    if(nVerbose) {
      std::cout << "encoding network" << std::endl;
    }
    pNtk->ForEachInt([&](int id) {
      EncodeNode(p, v, id);
    });
    // always care if it is po
    if(pNtk->IsPoDriver(id)) {
      return;
    }
    // store po vars (NOTE: it's not a lit)
    vLits.clear();
    pNtk->ForEachPoDriver([&](int fi) {
      vLits.push_back(v[fi]);
    });
    // encode an inverted copy
    if(nVerbose) {
      std::cout << "encoding an inverted copy" << std::endl;
    }
    pNtk->ForEachTfo(id, false, [&](int fo) {
      v[fo] = sat_solver_addvar(p);
      EncodeNode(p, v, fo, id);
    });
    // encode miter xors
    if(nVerbose) {
      std::cout << "encoding miter xors" << std::endl;
    }
    int idx = 0;
    int n = 0;
    pNtk->ForEachPoDriver([&](int fi) {
      assert(fi != id);
      if(v[fi] != vLits[idx]) {
        int x = sat_solver_addvar(p);
        if(nVerbose) {
          std::cout << x << " = " << v[fi] << " ^ " << vLits[idx] << std::endl;
        }
        RetValue = sat_solver_add_xor(p, x, v[fi], vLits[idx], 0);
        assert(RetValue);
        vLits[n] = toLitCond(x, 0);
        n++;
      }
      idx++;
    });
    vLits.resize(n);
    // assign or of xors to 1
    if(nVerbose) {
      std::cout << "adding miter output clause" << std::endl;
      std::cout << "(";
      std::string delim = "";
      for(int iLit: vLits) {
        std::cout << delim << (lit_sign(iLit)? "!": "") << lit_var(iLit);
        delim = ", ";
      }
      std::cout << ")" << std::endl;
    }
    if(n == 0) {
      status = false;
      return;
    }
    RetValue = sat_solver_addclause(p, vLits.data(), vLits.data() + n);
    assert(RetValue);
  }

  template <typename Ntk>
  void SatSolver<Ntk>::SetTarget(int id) {
    if(!fUpdate && id == target) {
      return;
    }
    fUpdate = false;
    target = id;
    EncodeMiter(pSat, vVars, target);
  }

  /* }}} */

  /* {{{ Constructors */

  template <typename Ntk>
  SatSolver<Ntk>::SatSolver(Parameter const *pPar) :
    pNtk(NULL),
    nVerbose(pPar->nSatSolverVerbose),
    nConflictLimit(pPar->nConflictLimit),
    pSat(sat_solver_new()),
    status(false),
    target(-1),
    fUpdate(false) {
    ResetSummary();
  }

  template <typename Ntk>
  SatSolver<Ntk>::~SatSolver() {
    sat_solver_delete(pSat);
    //std::cout << "SAT solver stats: calls = " << nCalls << " (SAT = " << nSats << ", UNSAT = " << nUnsats << ", UNDET = " << nCalls - nSats - nUnsats << ")" << std::endl;
  }

  template <typename Ntk>
  void SatSolver<Ntk>::AssignNetwork(Ntk *pNtk_, bool fReuse) {
    (void)fReuse;
    status = false;
    target = -1;
    fUpdate = false;
    pNtk = pNtk_;
    pNtk->AddCallback(std::bind(&SatSolver<Ntk>::ActionCallback, this, std::placeholders::_1));
  }

  /* }}} */

  /* {{{ Checks */
  
  template <typename Ntk>
  SatResult SatSolver<Ntk>::CheckRedundancy(int id, int idx) {
    time_point timeStart = GetCurrentTime();
    SetTarget(id);
    if(!status) {
      if(nVerbose) {
        std::cout << "trivially UNSATISFIABLE" << std::endl;
      }
      durationRedundancy += Duration(timeStart, GetCurrentTime());
      return UNSAT;
    }
    vLits.clear();
    assert(pNtk->GetNodeType(id) == AND);    
    pNtk->ForEachFaninIdx(id, [&](int idx2, int fi, bool c) {
      if(idx == idx2) {
        vLits.push_back(toLitCond(vVars[fi], !c));
      } else {
        vLits.push_back(toLitCond(vVars[fi], c));
      }
    });
    if(nVerbose) {
      std::cout << "solving with assumptions: ";
      std::string delim = "";
      for(int iLit: vLits) {
        std::cout << delim << (lit_sign(iLit)? "!": "") << lit_var(iLit);
        delim = ", ";
      }
      std::cout << std::endl;
    }
    nCalls++;
    int res = sat_solver_solve(pSat, vLits.data(), vLits.data() + vLits.size(), nConflictLimit, 0 /*nInsLimit*/, 0 /*nConfLimitGlobal*/, 0 /*nInsLimitGlobal*/);
    if(res == l_False) {
      if(nVerbose) {
        std::cout << "UNSATISFIABLE" << std::endl;
      }
      nUnsats++;
      durationRedundancy += Duration(timeStart, GetCurrentTime());
      return UNSAT;
    }
    if(res == l_Undef) {
      if(nVerbose) {
        std::cout << "UNDETERMINED" << std::endl;
      }
      durationRedundancy += Duration(timeStart, GetCurrentTime());
      return UNDET;
    }
    assert(res == l_True);
    if(nVerbose) {
      std::cout << "SATISFIABLE" << std::endl;
    }
    nSats++;
    vValues.clear();
    vValues.resize(pNtk->GetNumNodes());
    pNtk->ForEachPi([&](int id) {
      if(sat_solver_var_value(pSat, vVars[id])) {
        vValues[id] = TEMP_TRUE;
      } else {
        vValues[id] = TEMP_FALSE;
      }
    });
    pNtk->ForEachInt([&](int id) {
      if(sat_solver_var_value(pSat, vVars[id])) {
        vValues[id] = TEMP_TRUE;
      } else {
        vValues[id] = TEMP_FALSE;
      }
    });
    // required values
    pNtk->ForEachFaninIdx(id, [&](int idx2, int fi, bool c) {
      assert((vValues[fi] == TEMP_TRUE) ^ (idx == idx2) ^ c);
      vValues[fi] = DecideVarValue(vValues[fi]);
    });
    durationRedundancy += Duration(timeStart, GetCurrentTime());
    return SAT;
  }

  template <typename Ntk>
  SatResult SatSolver<Ntk>::CheckFeasibility(int id, int fi, bool c) {
    time_point timeStart = GetCurrentTime();
    SetTarget(id);
    if(!status) {
      if(nVerbose) {
        std::cout << "trivially UNSATISFIABLE" << std::endl;
      }
      durationFeasibility += Duration(timeStart, GetCurrentTime());
      return UNSAT;
    }
    vLits.clear();
    assert(pNtk->GetNodeType(id) == AND);
    vLits.push_back(toLit(vVars[id]));
    vLits.push_back(toLitCond(vVars[fi], !c));
    if(nVerbose) {
      std::cout << "solving with assumptions: ";
      std::string delim = "";
      for(int iLit: vLits) {
        std::cout << delim << (lit_sign(iLit)? "!": "") << lit_var(iLit);
        delim = ", ";
      }
      std::cout << std::endl;
    }
    nCalls++;
    int res = sat_solver_solve(pSat, vLits.data(), vLits.data() + vLits.size(), nConflictLimit, 0 /*nInsLimit*/, 0 /*nConfLimitGlobal*/, 0 /*nInsLimitGlobal*/);
    if(res == l_False) {
      if(nVerbose) {
        std::cout << "UNSATISFIABLE" << std::endl;
      }
      nUnsats++;
      durationFeasibility += Duration(timeStart, GetCurrentTime());
      return UNSAT;
    }
    if(res == l_Undef) {
      if(nVerbose) {
        std::cout << "UNDETERMINED" << std::endl;
      }
      durationFeasibility += Duration(timeStart, GetCurrentTime());
      return UNDET;
    }
    assert(res == l_True);
    if(nVerbose) {
      std::cout << "SATISFIABLE" << std::endl;
    }
    nSats++;
    vValues.clear();
    vValues.resize(pNtk->GetNumNodes());
    pNtk->ForEachPi([&](int id) {
      if(sat_solver_var_value(pSat, vVars[id])) {
        vValues[id] = TEMP_TRUE;
      } else {
        vValues[id] = TEMP_FALSE;
      }
    });
    pNtk->ForEachInt([&](int id) {
      if(sat_solver_var_value(pSat, vVars[id])) {
        vValues[id] = TEMP_TRUE;
      } else {
        vValues[id] = TEMP_FALSE;
      }
    });
    // required values
    assert(vValues[id] == TEMP_TRUE);
    vValues[id] = DecideVarValue(vValues[id]);
    assert((vValues[fi] == TEMP_TRUE) ^ !c);
    vValues[fi] = DecideVarValue(vValues[fi]);
    durationFeasibility += Duration(timeStart, GetCurrentTime());
    return SAT;
  }
  
  /* }}} */

  /* {{{ Cex */

  template <typename Ntk>
  std::vector<VarValue> SatSolver<Ntk>::GetCex() {
    if(nVerbose) {
      std::cout << "cex: ";
      pNtk->ForEachPi([&](int id) {
        std::cout << GetVarValueChar(vValues[id]);
      });
      std::cout << std::endl;
    }
    // reverse simulation
    pNtk->ForEachIntReverse([&](int id) {
      switch(pNtk->GetNodeType(id)) {
      case AND:
        if(vValues[id] == rrrTRUE) {
          pNtk->ForEachFanin(id, [&](int fi, bool c) {
            assert((vValues[fi] == TEMP_TRUE || vValues[fi] == rrrTRUE) ^ c);
            vValues[fi] = DecideVarValue(vValues[fi]);
          });
        } else if(vValues[id] == rrrFALSE) {
          bool fFound =  false;
          pNtk->ForEachFanin(id, [&](int fi, bool c) {
            if(fFound) {
              return;
            }
            if(c) {
              if(vValues[fi] == rrrTRUE) {
                fFound = true;
              }
            } else {
              if(vValues[fi] == rrrFALSE) {
                fFound = true;
              }
            }
          });
          if(!fFound) {
            pNtk->ForEachFanin(id, [&](int fi, bool c) {
              if(fFound) {
                return;
              }
              if(c) {
                if(vValues[fi] == TEMP_TRUE) {
                  fFound = true;
                  vValues[fi] = DecideVarValue(vValues[fi]);
                }
              } else {
                if(vValues[fi] == TEMP_FALSE) {
                  fFound = true;
                  vValues[fi] = DecideVarValue(vValues[fi]);
                }
              }
            });
          }
        }
        break;
      default:
        assert(0);
      }
    });
    if(nVerbose) {
      std::cout << "pex: ";
      pNtk->ForEachPi([&](int id) {
        std::cout << GetVarValueChar(vValues[id]);
      });
      std::cout << std::endl;
    }
    // debug
    pNtk->ForEachInt([&](int id) {
      switch(pNtk->GetNodeType(id)) {
      case AND:
        if(vValues[id] == rrrTRUE) {
          pNtk->ForEachFanin(id, [&](int fi, bool c) {
            assert(c || vValues[fi] == rrrTRUE);
            assert(!c || vValues[fi] == rrrFALSE);
          });
        } else if(vValues[id] == rrrFALSE) {
          bool fFound =  false;
          pNtk->ForEachFanin(id, [&](int fi, bool c) {
            if(fFound) {
              return;
            }
            if(c) {
              if(vValues[fi] == rrrTRUE) {
                fFound = true;
              }
            } else {
              if(vValues[fi] == rrrFALSE) {
                fFound = true;
              }
            }
          });
          assert(fFound);
        }
        break;
      default:
        assert(0);
      }
      });
    // retrieve partial cex
    std::vector<VarValue> vPartialCex;
    pNtk->ForEachPi([&](int id) {
      if(vValues[id] == rrrTRUE || vValues[id] == rrrFALSE) {
        vPartialCex.push_back(vValues[id]);
      } else {
        vPartialCex.push_back(UNDEF);
      }
    });
    return vPartialCex;
  }

  /* }}} */

  /* {{{ Stats */

  template <typename Ntk>
  void SatSolver<Ntk>::ResetSummary() {
    nCalls = 0;
    nSats = 0;
    nUnsats = 0;
    durationRedundancy = 0;
    durationFeasibility = 0;
  }

  template <typename Ntk>
  summary<int> SatSolver<Ntk>::GetStatsSummary() const {
    summary<int> v;
    v.emplace_back("sat call", nCalls);
    v.emplace_back("sat satisfiable", nSats);
    v.emplace_back("sat unsatisfiable", nUnsats);
    return v;
  }

  template <typename Ntk>
  summary<double> SatSolver<Ntk>::GetTimesSummary() const {
    summary<double> v;
    v.emplace_back("sat redundancy", durationRedundancy);
    v.emplace_back("sat feasibility", durationFeasibility);
    return v;
  }

  /* }}} */
  
}
