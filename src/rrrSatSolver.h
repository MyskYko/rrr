#pragma once

#include <vector>

#include <sat/bsat/satSolver.h>

#include "rrrTypes.h"
#include "rrrParameter.h"

namespace rrr {

  template <typename Ntk>
  class SatSolver {
  private:
    static const bool fVerbose = false;
    
    Ntk *pNtk;
    sat_solver *pSat;
    int status;
    int target;
    std::vector<int> vVars;
    std::vector<int> vLits; // temporary storage

    std::vector<VarValue> vValues;

    bool fUpdate;

    int nCalls;
    int nSats;
    int nUnsats;

    void ActionCallback(Action const &action);

    void EncodeNode(sat_solver *p, std::vector<int> const &v, int id, int to_negate = -1) const;
    void EncodeMiter(sat_solver *p, std::vector<int> &v, int id); // create a careset miter where the counterpart has the output of target negated
    void SetTarget(int id);
    
  public:
    SatSolver(Ntk *pNtk, Parameter const *pPar);
    ~SatSolver();
    void UpdateNetwork(Ntk *pNtk_);

    bool CheckRedundancy(int id, int idx);
    bool CheckFeasibility(int id, int fi, bool c);

    std::vector<VarValue> GetCex();
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

  /* }}} Create action callback end */
  
  /* {{{ Perform encoding */

  template <typename Ntk>
  void SatSolver<Ntk>::EncodeNode(sat_solver *p, std::vector<int> const &v, int id, int to_negate) const {
    int RetValue;
    int x = -1, y = -1;
    bool cx, cy;
    assert(pNtk->GetNodeType(id) == AND);
    pNtk->ForEachFanin(id, [&](int fi, bool c) {
      if(x == -1) {
        x = v[fi];
        cx = c ^ (fi == to_negate);
      } else if(y == -1) {
        y = v[fi];
        cy = c ^ (fi == to_negate);
      } else {
        int z = sat_solver_addvar(p);
        if(fVerbose) {
          std::cout << z << " = " << (cx? "!": "") << x << " & " << (cy? "!": "") << y << std::endl;
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
      if(fVerbose) {
        std::cout << v[id] << " = !0" << std::endl;
      }
      RetValue = sat_solver_add_const(p, v[id], 0);
      assert(RetValue);
    } else if(y == -1) {
      if(fVerbose) {
        std::cout << v[id] << " = " << (cx? "!": "") << x << std::endl;
      }
      RetValue = sat_solver_add_buffer(p, v[id], x, cx);
      assert(RetValue);
    } else {
      if(fVerbose) {
        std::cout << v[id] << " = " << (cx? "!": "") << x << " & " << (cy? "!": "") << y << std::endl;
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
    status = 1;
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
    pNtk->ForEachInt([&](int id) {
      EncodeNode(p, v, id);
    });
    // always care if it is po
    if(pNtk->IsPoDriver(id)) {
      return;
    }
    // store po vars (NOTE: it's not a lit)
    vLits.clear();
    pNtk->ForEachPoDriver([&](int fi, bool c) {
      vLits.push_back(v[fi]);
    });
    // encode an inverted copy
    pNtk->ForEachTfo(id, false, [&](int fo) {
      v[fo] = sat_solver_addvar(p);
      EncodeNode(p, v, fo, id);
    });
    // encode xors
    int idx = 0;
    int n = 0;
    pNtk->ForEachPoDriver([&](int fi, bool c) {
      assert(fi != id);
      if(v[fi] != vLits[idx]) {
        int x = sat_solver_addvar(p);
        if(fVerbose) {
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
    if(fVerbose) {
      std::cout << "(";
      std::string delim = "";
      for(int iLit: vLits) {
        std::cout << delim << (lit_sign(iLit)? "!": "") << lit_var(iLit);
        delim = ", ";
      }
      std::cout << ")" << std::endl;
    }
    if(n == 0) {
      status = 0;
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

  /* }}} Perform encoding end */

  /* {{{ Constructor */

  template <typename Ntk>
  SatSolver<Ntk>::SatSolver(Ntk *pNtk, Parameter const *pPar) :
    pNtk(pNtk),
    pSat(sat_solver_new()),
    status(0),
    target(-1),
    fUpdate(false),
    nCalls(0),
    nSats(0),
    nUnsats(0) {
    pNtk->AddCallback(std::bind(&SatSolver<Ntk>::ActionCallback, this, std::placeholders::_1));
  }

  template <typename Ntk>
  SatSolver<Ntk>::~SatSolver() {
    sat_solver_delete(pSat);
    std::cout << "solver stats: SAT calls = " << nCalls << ", SAT = " << nSats << ", UNSAT = " << nUnsats << std::endl;
  }

  template <typename Ntk>
  void SatSolver<Ntk>::UpdateNetwork(Ntk *pNtk_) {
    pNtk = pNtk_;
    assert(0);
  }

  /* }}} Constructor end */

  /* {{{ Perform checks */
  
  template <typename Ntk>
  bool SatSolver<Ntk>::CheckRedundancy(int id, int idx) {
    SetTarget(id);
    if(!status) {
      if(fVerbose) {
        std::cout << "trivially UNSAT" << std::endl;
      }
      return true;
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
    if(fVerbose) {
      std::cout << "assumption: ";
      std::string delim = "";
      for(int iLit: vLits) {
        std::cout << delim << (lit_sign(iLit)? "!": "") << lit_var(iLit);
        delim = ", ";
      }
      std::cout << std::endl;
    }
    nCalls++;
    int res = sat_solver_solve(pSat, vLits.data(), vLits.data() + vLits.size(), 0 /*nConfLimit*/, 0 /*nInsLimit*/, 0 /*nConfLimitGlobal*/, 0 /*nInsLimitGlobal*/);
    if(res == l_False) {
      nUnsats++;
      return true;
    }
    //if(res == l_Undef) ;
    assert(res == l_True);
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
    return false;
  }

  template <typename Ntk>
  bool SatSolver<Ntk>::CheckFeasibility(int id, int fi, bool c) {
    SetTarget(id);
    if(!status) {
      if(fVerbose) {
        std::cout << "trivially UNSAT" << std::endl;
      }
      return true;
    }
    vLits.clear();
    assert(pNtk->GetNodeType(id) == AND);
    vLits.push_back(toLit(vVars[id]));
    vLits.push_back(toLitCond(vVars[fi], !c));
    if(fVerbose) {
      std::cout << "assumption: ";
      std::string delim = "";
      for(int iLit: vLits) {
        std::cout << delim << (lit_sign(iLit)? "!": "") << lit_var(iLit);
        delim = ", ";
      }
      std::cout << std::endl;
    }
    nCalls++;
    int res = sat_solver_solve(pSat, vLits.data(), vLits.data() + vLits.size(), 0 /*nConfLimit*/, 0 /*nInsLimit*/, 0 /*nConfLimitGlobal*/, 0 /*nInsLimitGlobal*/);
    if(res == l_False) {
      nUnsats++;
      return true;
    }
    //if(res == l_Undef) ;
    assert(res == l_True);
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
    return false;
  }
  
  /* }}} Perform checks end */

  /* {{{ Cex */

  template <typename Ntk>
  std::vector<VarValue> SatSolver<Ntk>::GetCex() {
    if(fVerbose) {
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
        if(vValues[id] == TRUE) {
          pNtk->ForEachFanin(id, [&](int fi, bool c) {
            assert((vValues[fi] == TEMP_TRUE || vValues[fi] == TRUE) ^ c);
            vValues[fi] = DecideVarValue(vValues[fi]);
          });
        } else if(vValues[id] == FALSE) {
          bool fFound =  false;
          pNtk->ForEachFanin(id, [&](int fi, bool c) {
            if(fFound) {
              return;
            }
            if(c) {
              if(vValues[fi] == TRUE) {
                fFound = true;
              }
            } else {
              if(vValues[fi] == FALSE) {
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
    if(fVerbose) {
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
        if(vValues[id] == TRUE) {
          pNtk->ForEachFanin(id, [&](int fi, bool c) {
            assert(c || vValues[fi] == TRUE);
            assert(!c || vValues[fi] == FALSE);
          });
        } else if(vValues[id] == FALSE) {
          bool fFound =  false;
          pNtk->ForEachFanin(id, [&](int fi, bool c) {
            if(fFound) {
              return;
            }
            if(c) {
              if(vValues[fi] == TRUE) {
                fFound = true;
              }
            } else {
              if(vValues[fi] == FALSE) {
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
      if(vValues[id] == TRUE || vValues[id] == FALSE) {
        vPartialCex.push_back(vValues[id]);
      } else {
        vPartialCex.push_back(UNDEF);
      }
    });
    return vPartialCex;
  }

  /* }}} Cex end */

}
