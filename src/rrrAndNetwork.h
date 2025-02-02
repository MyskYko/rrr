#pragma once

#include <iostream>
#include <vector>
#include <list>
#include <set>
#include <initializer_list>
#include <string>
#include <functional>
#include <algorithm>

#include "rrrTypes.h"


namespace rrr {

  class AndNetwork {
  private:
    using ite = std::list<int>::iterator;
    using cite = std::list<int>::const_iterator;
    using Callback = std::function<void(Action)>;
    
    int nNodes;
    std::vector<int> vPis;
    std::list<int> lsInts; // internal nodes in topological order
    std::set<int> sInts;
    std::vector<int> vPos;
    std::vector<std::vector<int> > vvFaninEdges; // complementable edges, no duplicated fanins allowed
    std::vector<int> vRefs; // reference count

    bool fLockTrav;
    unsigned  iTrav;
    std::vector<unsigned> vTrav;

    bool fPropagating;
    
    std::vector<Callback> vCallbacks;

    std::vector<AndNetwork> vBackups;

    int  Node2Edge(int id, bool c) const { return (id << 1) + (int)c; }
    int  Edge2Node(int f)          const { return f >> 1;             }
    bool EdgeIsCompl(int f)        const { return f & 1;              }

    int  CreateNode();

    void SortInts(ite it);
    void TakenAction(Action action);

  public:
    AndNetwork(int nReserve = 1);
    AndNetwork(AndNetwork const &x);
    AndNetwork &operator=(AndNetwork const &x);

    int  GetConst0() const;
    int  AddPi();
    int  AddPo(int id, bool c);
    int  AddAnd(int id0, int id1, bool c0, bool c1);

    bool UseComplementedEdges() const;
    int  GetNumNodes() const; // number of allocated nodes (max id + 1)
    int  GetNumPis() const;
    int  GetNumInts() const;
    int  GetNumPos() const;
    int  GetPi(int idx) const;
    std::vector<int> GetPis() const;
    std::vector<int> GetInts() const;
    std::vector<int> GetPos() const;
    
    NodeType GetNodeType(int id) const;
    bool IsPi(int id) const;
    bool IsInt(int id) const;
    bool IsPo(int id) const;
    bool IsPoDriver(int id) const;
    int  GetNumFanins(int id) const;
    int  GetNumFanouts(int id) const;
    int  FindFanin(int id, int fi) const;
    int  GetFanin(int id, int idx) const;
    bool GetCompl(int id, int idx) const;
    bool IsReconvergent(int id);
    
    void ForEachPi(std::function<void(int)> func) const;
    void ForEachInt(std::function<void(int)> func) const;
    void ForEachIntReverse(std::function<void(int)> func) const;
    void ForEachPo(std::function<void(int)> func) const;
    void ForEachPoDriver(std::function<void(int, bool)> func) const;
    void ForEachFanin(int id, std::function<void(int, bool)> func) const;
    void ForEachFaninIdx(int id, std::function<void(int, int, bool)> func) const;
    void ForEachFanout(int id, std::function<void(int, bool)> func) const;
    void ForEachFanoutIdx(int id, std::function<void(int, bool, int)> func) const;
    void ForEachTfo(int id, bool fPos, std::function<void(int)> func);
    void ForEachTfoReverse(int id, bool fPos, std::function<void(int)> func);
    void ForEachTfoUpdate(int id, bool fPos, std::function<bool(int)> func);
    template <template <typename> typename Container>
    void ForEachTfos(Container<int> const &ids, bool fPos, std::function<void(int)> func);
    template <template <typename> typename Container>
    void ForEachTfosUpdate(Container<int> const &ids, bool fPos, std::function<bool(int)> func);

    void AddCallback(Callback callback);

    void TrivialCollapse(int id);
    void TrivialDecompose(int id);

    void AddFanin(int id, int fi, bool c);
    void RemoveFanin(int id, int idx);
    void RemoveUnused(int id, bool fRecursive = false);
    void RemoveBuffer(int id);
    void RemoveConst(int id);
    void Propagate(int id = -1); // all nodes unless specified
    void Sweep(bool fPropagate);

    int  Save(int slot = -1); // slot is assigned automatically unless specified
    void Load(int slot);
    void PopBack();

    void Print() const;
  };


  /* {{{ Private functions */

  int AndNetwork::CreateNode() {
    vvFaninEdges.emplace_back();
    vRefs.push_back(0);
    return nNodes++;
  }

  void AndNetwork::SortInts(ite it) {
    ForEachFanin(*it, [&](int fi, bool c) {
      ite it2 = std::find(it, lsInts.end(), fi);
      if(it2 != lsInts.end()) {
        lsInts.erase(it2);
        it2 = lsInts.insert(it, fi);
        SortInts(it2);
      }
    });
  }
  
  void AndNetwork::TakenAction(Action action) {
    //PrintAction(action);
    for(Callback &callback: vCallbacks) {
      callback(action);
    }
  }

  /* }}} Private functions end */

  /* {{{ Constructor */

  AndNetwork::AndNetwork(int nReserve) :
    nNodes(0),
    fLockTrav(false),
    iTrav(0),
    fPropagating(false) {
    vvFaninEdges.reserve(nReserve);
    vRefs.reserve(nReserve);
    // add const-0 node
    vvFaninEdges.emplace_back();
    vRefs.push_back(0);
    nNodes++;
  }

  AndNetwork::AndNetwork(AndNetwork const &x) {
    nNodes   = x.nNodes;
    vPis     = x.vPis;
    lsInts   = x.lsInts;
    sInts    = x.sInts;
    vPos     = x.vPos;
    vvFaninEdges = x.vvFaninEdges;
    vRefs    = x.vRefs;
  }
  
  AndNetwork &AndNetwork::operator=(AndNetwork const &x) {
    nNodes   = x.nNodes;
    vPis     = x.vPis;
    lsInts   = x.lsInts;
    sInts    = x.sInts;
    vPos     = x.vPos;
    vvFaninEdges = x.vvFaninEdges;
    vRefs    = x.vRefs;
    return *this;
  }
  
  /* }}} Constructor end */

  /* {{{ Construction APIs */

  int AndNetwork::GetConst0() const {
    return 0;
  }
  
  int AndNetwork::AddPi() {
    vPis.push_back(nNodes);
    vvFaninEdges.emplace_back();
    vRefs.push_back(0);
    return nNodes++;
  }
  
  int AndNetwork::AddAnd(int id0, int id1, bool c0, bool c1) {
    assert(id0 < nNodes);
    assert(id1 < nNodes);
    assert(id0 != id1);
    lsInts.push_back(nNodes);
    sInts.insert(nNodes);
    vRefs[id0]++;
    vRefs[id1]++;
    vvFaninEdges.emplace_back((std::initializer_list<int>){Node2Edge(id0, c0), Node2Edge(id1, c1)});
    vRefs.push_back(0);
    return nNodes++;
  }

  int AndNetwork::AddPo(int id, bool c) {
    assert(id < nNodes);
    vPos.push_back(nNodes);
    vRefs[id]++;
    vvFaninEdges.emplace_back((std::initializer_list<int>){Node2Edge(id, c)});
    vRefs.push_back(0);
    return nNodes++;
  }

  /* }}} Construction APIs end */
  
  /* {{{ Network properties */
  
  inline bool AndNetwork::UseComplementedEdges() const {
    return true;
  }
  
  inline int AndNetwork::GetNumNodes() const {
    return nNodes;
  }

  inline int AndNetwork::GetNumPis() const {
    return vPis.size();
  }
  
  inline int AndNetwork::GetNumInts() const {
    return lsInts.size();
  }
  
  inline int AndNetwork::GetNumPos() const {
    return vPos.size();
  }

  inline int AndNetwork::GetPi(int idx) const {
    return vPis[idx];
  }

  std::vector<int> AndNetwork::GetPis() const {
    return vPis;
  }

  std::vector<int> AndNetwork::GetInts() const {
    return std::vector<int>(lsInts.begin(), lsInts.end());
  }
  
  std::vector<int> AndNetwork::GetPos() const {
    return vPos;
  }
  
  /* }}} Network properties end */

  /* {{{ Node properties */

  inline NodeType AndNetwork::GetNodeType(int id) const {
    if(IsPi(id)) {
      return PI;
    }
    if(IsPo(id)) {
      return PO;
    }
    return AND;
  }

  inline bool AndNetwork::IsPi(int id) const {
    return GetNumFanins(id) == 0 && std::find(vPis.begin(), vPis.end(), id) != vPis.end();
  }

  inline bool AndNetwork::IsInt(int id) const {
    return sInts.count(id);
  }

  inline bool AndNetwork::IsPo(int id) const {
    return GetNumFanouts(id) == 0 && std::find(vPos.begin(), vPos.end(), id) != vPos.end();
  }
  
  inline bool AndNetwork::IsPoDriver(int id) const {
    for(int po: vPos) {
      if(GetFanin(po, 0) == id) {
        return true;
      }
    }
    return false;
  }
  
  inline int AndNetwork::GetNumFanins(int id) const {
    return vvFaninEdges[id].size();
  }

  inline int AndNetwork::GetNumFanouts(int id) const {
    return vRefs[id];
  }
  
  inline int AndNetwork::FindFanin(int id, int fi) const {
    int idx = 0;
    for(int fi_edge: vvFaninEdges[id]) {
      if(Edge2Node(fi_edge) == fi) {
        return idx;
      }
      idx++;
    }
    return -1;
  }
  
  inline int AndNetwork::GetFanin(int id, int idx) const {
    return Edge2Node(vvFaninEdges[id][idx]);
  }

  inline bool AndNetwork::GetCompl(int id, int idx) const {
    return EdgeIsCompl(vvFaninEdges[id][idx]);
  }

  bool AndNetwork::IsReconvergent(int id) {
    if(GetNumFanouts(id) <= 1) {
      return false;
    }
    assert(!fLockTrav);
    fLockTrav = true;
    vTrav.resize(nNodes);
    int iTravStart = iTrav;
    ForEachFanout(id, [&](int fo, bool c) {
      iTrav++;
      assert(iTrav != 0);
      vTrav[fo] = iTrav;
    });
    cite it = lsInts.begin();
    while(vTrav[*it] <= iTravStart && it != lsInts.end()) {
      it++;
    }
    it++;
    for(; it != lsInts.end(); it++) {
      for(int fi_edge: vvFaninEdges[*it]) {
        if(vTrav[Edge2Node(fi_edge)] > iTravStart) {
          if(vTrav[*it] > iTravStart && vTrav[*it] != vTrav[Edge2Node(fi_edge)]) {
            fLockTrav = false;
            return true;
          }
          vTrav[*it] = vTrav[Edge2Node(fi_edge)];
        }
      }
    }
    fLockTrav = false;
    return false;
  }

  /* }}} Node properties end */

  /* {{{ Traverse elements */

  inline void AndNetwork::ForEachPi(std::function<void(int)> func) const {
    for(int pi: vPis) {
      func(pi);
    }
  }
  
  inline void AndNetwork::ForEachInt(std::function<void(int)> func) const {
    for(int id: lsInts) {
      func(id);
    }
  }

  inline void AndNetwork::ForEachIntReverse(std::function<void(int)> func) const {
    for(std::list<int>::const_reverse_iterator it = lsInts.rbegin(); it != lsInts.rend(); it++) {
      func(*it);
    }
  }

  inline void AndNetwork::ForEachPo(std::function<void(int)> func) const {
    for(int po: vPos) {
      func(po);
    }
  }
  
  inline void AndNetwork::ForEachPoDriver(std::function<void(int, bool)> func) const {
    for(int po: vPos) {
      func(GetFanin(po, 0), GetCompl(po, 0));
    }
  }
  
  inline void AndNetwork::ForEachFanin(int id, std::function<void(int, bool)> func) const {
    for(int fi_edge: vvFaninEdges[id]) {
      func(Edge2Node(fi_edge), EdgeIsCompl(fi_edge));
    }
  }

  inline void AndNetwork::ForEachFaninIdx(int id, std::function<void(int, int, bool)> func) const {
    int idx = 0;
    for(int fi_edge: vvFaninEdges[id]) {
      func(idx, Edge2Node(fi_edge), EdgeIsCompl(fi_edge));
      idx++;
    }
  }
  
  inline void AndNetwork::ForEachFanout(int id, std::function<void(int, bool)> func) const {
    if(vRefs[id] == 0) {
      return;
    }
    cite it = std::find(lsInts.begin(), lsInts.end(), id);
    assert(it != lsInts.end());
    it++;
    int nRefs = 0;
    for(; nRefs < vRefs[id] && it != lsInts.end(); it++) {
      int idx = FindFanin(*it, id);
      if(idx >= 0) {
        func(*it, GetCompl(*it, idx));
        nRefs++;
      }
    }
    if(nRefs < vRefs[id]) {
      for(int po: vPos) {
        if(GetFanin(po, 0) == id) {
          func(po, GetCompl(po, 0));
          nRefs++;
          if(nRefs == vRefs[id]) {
            break;
          }
        }
      }
    }
    assert(nRefs == vRefs[id]);
  }
  
  inline void AndNetwork::ForEachFanoutIdx(int id, std::function<void(int, bool, int)> func) const {
    if(vRefs[id] == 0) {
      return;
    }
    cite it = std::find(lsInts.begin(), lsInts.end(), id);
    assert(it != lsInts.end());
    it++;
    int nRefs = 0;
    for(; nRefs < vRefs[id] && it != lsInts.end(); it++) {
      int idx = FindFanin(*it, id);
      if(idx >= 0) {
        func(*it, GetCompl(*it, idx), idx);
        nRefs++;
      }
    }
    if(nRefs < vRefs[id]) {
      for(int po: vPos) {
        if(GetFanin(po, 0) == id) {
          func(po, GetCompl(po, 0), 0);
          nRefs++;
          if(nRefs == vRefs[id]) {
            break;
          }
        }
      }
    }
    assert(nRefs == vRefs[id]);
  }
  
  void AndNetwork::ForEachTfo(int id, bool fPos, std::function<void(int)> func) {
    // this does not include id itself
    if(vRefs[id] == 0) {
      return;
    }
    cite it = std::find(lsInts.begin(), lsInts.end(), id);
    assert(it != lsInts.end());
    assert(!fLockTrav);
    fLockTrav = true;
    vTrav.resize(nNodes);
    iTrav++;
    assert(iTrav != 0);
    vTrav[id] = iTrav;
    it++;
    for(; it != lsInts.end(); it++) {
      for(int fi_edge: vvFaninEdges[*it]) {
        if(vTrav[Edge2Node(fi_edge)] == iTrav) {
          func(*it);
          vTrav[*it] = iTrav;
          break;
        }
      }
    }
    if(fPos) {
      for(int po: vPos) {
        if(vTrav[GetFanin(po, 0)] == iTrav) {
          func(po);
          vTrav[po] = iTrav;
        }
      }
    }
    fLockTrav = false;
  }

  void AndNetwork::ForEachTfoReverse(int id, bool fPos, std::function<void(int)> func) {
    // this does not include id itself
    if(vRefs[id] == 0) {
      return;
    }
    cite it = std::find(lsInts.begin(), lsInts.end(), id);
    assert(it != lsInts.end());
    assert(!fLockTrav);
    fLockTrav = true;
    vTrav.resize(nNodes);
    iTrav++;
    assert(iTrav != 0);
    vTrav[id] = iTrav;
    it++;
    for(; it != lsInts.end(); it++) {
      for(int fi_edge: vvFaninEdges[*it]) {
        if(vTrav[Edge2Node(fi_edge)] == iTrav) {
          vTrav[*it] = iTrav;
          break;
        }
      }
    }
    if(fPos) {
      for(int po: vPos) {
        if(vTrav[GetFanin(po, 0)] == iTrav) {
          func(po);
          vTrav[po] = iTrav;
        }
      }
    }
    fLockTrav = false;
    std::list<int>::const_reverse_iterator end = std::find(lsInts.rbegin(), lsInts.rend(), id);
    for(std::list<int>::const_reverse_iterator it = lsInts.rbegin(); it != end; it++) {
      func(*it);
    }
  }

  void AndNetwork::ForEachTfoUpdate(int id, bool fPos, std::function<bool(int)> func) {
    // this does not include id itself
    if(vRefs[id] == 0) {
      return;
    }
    cite it = std::find(lsInts.begin(), lsInts.end(), id);
    assert(it != lsInts.end());
    assert(!fLockTrav);
    fLockTrav = true;
    vTrav.resize(nNodes);
    iTrav++;
    assert(iTrav != 0);
    vTrav[id] = iTrav;
    it++;
    for(; it != lsInts.end(); it++) {
      for(int fi_edge: vvFaninEdges[*it]) {
        if(vTrav[Edge2Node(fi_edge)] == iTrav) {
          if(func(*it)) {
            vTrav[*it] = iTrav;
          }
          break;
        }
      }
    }
    if(fPos) {
      for(int po: vPos) {
        if(vTrav[GetFanin(po, 0)] == iTrav) {
          if(func(po)) {
            vTrav[po] = iTrav;
          }
        }
      }
    }
    fLockTrav = false;
  }

  template <template <typename> typename Container>
  void AndNetwork::ForEachTfos(Container<int> const &ids, bool fPos, std::function<void(int)> func) {
    // this includes ids themselves
    assert(!fLockTrav);
    fLockTrav = true;
    vTrav.resize(nNodes);
    iTrav++;
    assert(iTrav != 0);
    for(int id: ids) {
      vTrav[id] = iTrav;
    }
    cite it = lsInts.begin();
    while(vTrav[*it] != iTrav && it != lsInts.end()) {
      it++;
    }
    for(; it != lsInts.end(); it++) {
      if(vTrav[*it] == iTrav) {
        func(*it);
      } else {
        for(int fi_edge: vvFaninEdges[*it]) {
          if(vTrav[Edge2Node(fi_edge)] == iTrav) {
            func(*it);
            vTrav[*it] = iTrav;
            break;
          }
        }
      }
    }
    if(fPos) {
      for(int po: vPos) {
        if(vTrav[po] == iTrav || vTrav[GetFanin(po, 0)] == iTrav) {
          func(po);
          vTrav[po] = iTrav;
        }
      }
    }
    fLockTrav = false;
  }
  
  template <template <typename> typename Container>
  void AndNetwork::ForEachTfosUpdate(Container<int> const &ids, bool fPos, std::function<bool(int)> func) {
    // this includes ids themselves
    assert(!fLockTrav);
    fLockTrav = true;
    vTrav.resize(nNodes);
    iTrav++;
    assert(iTrav != 0);
    for(int id: ids) {
      vTrav[id] = iTrav;
    }
    cite it = lsInts.begin();
    while(vTrav[*it] != iTrav && it != lsInts.end()) {
      it++;
    }
    for(; it != lsInts.end(); it++) {
      if(vTrav[*it] == iTrav) {
        if(!func(*it)) {
          vTrav[*it] = 0;
        }
      } else {
        for(int fi_edge: vvFaninEdges[*it]) {
          if(vTrav[Edge2Node(fi_edge)] == iTrav) {
            if(func(*it)) {
              vTrav[*it] = iTrav;
            }
            break;
          }
        }
      }
    }
    if(fPos) {
      for(int po: vPos) {
        if(vTrav[po] == iTrav) {
          if(!func(po)) {
            vTrav[po] = 0;
          }
        } else if(vTrav[GetFanin(po, 0)] == iTrav) {
          if(func(po)) {
            vTrav[po] = iTrav;
          }
        }
      }
    }
    fLockTrav = false;
  }

  /* }}} Traverse elements end */
  
  /* {{{ Add callbacks */
  
  void AndNetwork::AddCallback(Callback callback) {
    vCallbacks.push_back(callback);
  }
  
  /* }}} Add callbacks end  */

  /* {{{ Modify network */

  void AndNetwork::TrivialCollapse(int id) {
    for(int idx = 0; idx < GetNumFanins(id);) {
      int fi_edge = vvFaninEdges[id][idx];
      int fi = Edge2Node(fi_edge);
      bool c = EdgeIsCompl(fi_edge);
      if(!IsPi(fi) && !c && vRefs[fi] == 1) {
        Action action;
        action.type = TRIVIAL_COLLAPSE;
        action.id = id;
        action.idx = idx;
        action.fi = fi;
        action.c = c;
        std::vector<int>::iterator it = vvFaninEdges[id].begin() + idx;
        it = vvFaninEdges[id].erase(it);
        vvFaninEdges[id].insert(it, vvFaninEdges[fi].begin(), vvFaninEdges[fi].end());
        ForEachFanin(fi, [&](int fi, bool c) {
          action.vFanins.push_back(fi);
        });
        // remove collapsed fanin
        vRefs[fi] = 0;
        vvFaninEdges[fi].clear();
        lsInts.erase(std::find(lsInts.begin(), lsInts.end(), fi));
        sInts.erase(fi);
        TakenAction(action);
      } else {
        idx++;
      }
    }
  }

  void AndNetwork::TrivialDecompose(int id) {
    while(GetNumFanins(id) > 2) {
      Action action;
      action.type = TRIVIAL_DECOMPOSE;
      action.id = id;
      action.idx = vvFaninEdges[id].size() - 2;
      int new_fi = CreateNode();
      action.fi = new_fi;
      action.c = false;
      int fi_edge1 = vvFaninEdges[id].back();
      vvFaninEdges[id].pop_back();
      int fi_edge0 = vvFaninEdges[id].back();
      vvFaninEdges[id].pop_back();
      vvFaninEdges[new_fi].push_back(fi_edge0);
      action.vFanins.push_back(Edge2Node(fi_edge0));
      vvFaninEdges[new_fi].push_back(fi_edge1);
      action.vFanins.push_back(Edge2Node(fi_edge1));
      vvFaninEdges[id].push_back(Node2Edge(new_fi, false));
      vRefs[new_fi]++;
      ite it = std::find(lsInts.begin(), lsInts.end(), id);
      lsInts.insert(it, new_fi);
      sInts.insert(new_fi);
      TakenAction(action);
    }
  }

  void AndNetwork::AddFanin(int id, int fi, bool c) {
    assert(FindFanin(id, fi) == -1); // no duplication
    assert(fi != GetConst0() || !c); // no const-1
    Action action;
    action.type = ADD_FANIN;
    action.id = id;
    action.idx = vvFaninEdges[id].size();
    action.fi = fi;
    action.c = c;
    ite it = std::find(lsInts.begin(), lsInts.end(), id);
    ite it2 = std::find(it, lsInts.end(), fi);
    if(it2 != lsInts.end()) {
      lsInts.erase(it2);
      it2 = lsInts.insert(it, fi);
      SortInts(it2);
    }
    vRefs[fi]++;
    vvFaninEdges[id].push_back(Node2Edge(fi, c));
    TakenAction(action);
  }
  
  void AndNetwork::RemoveFanin(int id, int idx) {
    Action action;
    action.type = REMOVE_FANIN;
    action.id = id;
    action.idx = idx;
    int fi = GetFanin(id, idx);
    bool c = GetCompl(id, idx);
    action.fi = fi;
    action.c = c;
    vRefs[fi]--;
    vvFaninEdges[id].erase(vvFaninEdges[id].begin() + idx);
    TakenAction(action);
  }

  void AndNetwork::RemoveUnused(int id, bool fRecursive) {
    assert(vRefs[id] == 0);
    Action action;
    action.type = REMOVE_UNUSED;
    action.id = id;
    ForEachFanin(id, [&](int fi, bool c) {
      action.vFanins.push_back(fi);
      vRefs[fi]--;
    });
    vvFaninEdges[id].clear();
    ite it = std::find(lsInts.begin(), lsInts.end(), id);
    lsInts.erase(it);
    sInts.erase(id);
    TakenAction(action);
    if(fRecursive) {
      for(int fi: action.vFanins) {
        if(vRefs[fi] == 0) {
          RemoveUnused(fi, true);
        }
      }
    }
  }

  void AndNetwork::RemoveBuffer(int id) {
    assert(GetNumFanins(id) == 1);
    assert(!fPropagating || fLockTrav);
    Action action;
    action.type = REMOVE_BUFFER;
    action.id = id;
    action.idx = 0;
    int fi = GetFanin(id, 0);
    bool c = GetCompl(id, 0);
    action.fi = fi;
    action.c = c;
    ForEachFanoutIdx(id, [&](int fo, bool foc, int idx) {
      action.vFanouts.push_back(fo);
      // duplicate
      int idx2 = FindFanin(fo, fi);
      if(idx2 != -1) {
        if(GetCompl(fo, idx2) == (c ^ foc)) {
          // it already exists, so just remove one
          vvFaninEdges[fo].erase(vvFaninEdges[fo].begin() + idx);
          if(fPropagating && GetNumFanins(fo) == 1) {
            vTrav[fo] = iTrav;
          }
        } else {
          // add const-0
          vRefs[fi]--;
          vRefs[GetConst0()]++;
          if(idx < idx2) {
            vvFaninEdges[fo][idx] = Node2Edge(GetConst0(), 0);
            vvFaninEdges[fo].erase(vvFaninEdges[fo].begin() + idx2);
          } else {
            assert(idx != idx2);
            vvFaninEdges[fo][idx2] = Node2Edge(GetConst0(), 0);
            vvFaninEdges[fo].erase(vvFaninEdges[fo].begin() + idx);
          }
          if(fPropagating) {
            vTrav[fo] = iTrav;
          }
        }
        return;
      }
      // const
      if(fi == GetConst0()) {
        assert(!c);
        if(foc) {
          // just remove const-1
          vvFaninEdges[fo].erase(vvFaninEdges[fo].begin() + idx);
          // exception is po
          if(GetNumFanins(fo) == 0 && IsPo(fo)) {
            vRefs[GetConst0()]++;
            vvFaninEdges[fo].push_back(Node2Edge(GetConst0(), true));
          }
          if(fPropagating && GetNumFanins(fo) <= 1) {
            vTrav[fo] = iTrav;
          }
        } else {
          // add const-0
          vRefs[GetConst0()]++;
          vvFaninEdges[fo][idx] = Node2Edge(GetConst0(), 0);
          if(fPropagating) {
            vTrav[fo] = iTrav;
          }
        }
        return;
      }
      // otherwise
      vvFaninEdges[fo][idx] = Node2Edge(fi, c ^ foc);
      vRefs[fi]++;
    });
    vRefs[id] = 0;
    vRefs[fi]--;
    vvFaninEdges[id].clear();
    if(!fPropagating) {
      ite it = std::find(lsInts.begin(), lsInts.end(), id);
      lsInts.erase(it);
    }
    sInts.erase(id);
    TakenAction(action);
  }

  void AndNetwork::RemoveConst(int id) {
    assert(GetNumFanins(id) == 0 || FindFanin(id, GetConst0()) != -1);
    assert(!fPropagating || fLockTrav);
    Action action;
    action.type = REMOVE_CONST;
    action.id = id;
    ForEachFanin(id, [&](int fi, bool c) {
      assert(fi != GetConst0() || !c);
      vRefs[fi]--;
      action.vFanins.push_back(fi);
    });
    bool c = (GetNumFanins(id) == 0);
    ForEachFanoutIdx(id, [&](int fo, bool foc, int idx) {
      action.vFanouts.push_back(fo);
      if(c ^ foc) {
        // just remove const-1
        vvFaninEdges[fo].erase(vvFaninEdges[fo].begin() + idx);
        // exception is po
        if(GetNumFanins(fo) == 0 && IsPo(fo)) {
          vRefs[GetConst0()]++;
          vvFaninEdges[fo].push_back(Node2Edge(GetConst0(), true));
        }
        if(fPropagating && GetNumFanins(fo) <= 1) {
          vTrav[fo] = iTrav;
        }
      } else {
        // add const-0
        vRefs[GetConst0()]++;
        vvFaninEdges[fo][idx] = Node2Edge(GetConst0(), 0);
        if(fPropagating) {
          vTrav[fo] = iTrav;
        }
      }
    });
    vRefs[id] = 0;
    vvFaninEdges[id].clear();
    if(!fPropagating) {
      ite it = std::find(lsInts.begin(), lsInts.end(), id);
      lsInts.erase(it);
    }
    sInts.erase(id);
    TakenAction(action);
  }

  void AndNetwork::Propagate(int id) {
    assert(!fLockTrav);
    fLockTrav = true;
    vTrav.resize(nNodes);
    iTrav++;
    assert(iTrav != 0);
    ite it;
    if(id == -1) {
      ForEachInt([&](int id) {
        if(GetNumFanins(id) <= 1 || FindFanin(id, GetConst0()) != -1) {
          vTrav[id] = iTrav;
        }
      });
      for(it = lsInts.begin(); it != lsInts.end(); it++) {
        if(vTrav[*it] == iTrav) {
          break;
        }
      }
    } else {
      vTrav[id] = iTrav;
      it = std::find(lsInts.begin(), lsInts.end(), id);
    }
    fPropagating = true;
    while(it != lsInts.end()) {
      if(vTrav[*it] == iTrav) {
        if(GetNumFanins(*it) == 1) {
          RemoveBuffer(*it);
        } else {
          RemoveConst(*it);
        }
        it = lsInts.erase(it);
      } else {
        it++;
      }
    }
    fPropagating = false;
    fLockTrav = false;
  }

  void AndNetwork::Sweep(bool fPropagate) {
    if(fPropagate) {
      Propagate();
    }
    for(std::list<int>::reverse_iterator it = lsInts.rbegin(); it != lsInts.rend();) {
      if(vRefs[*it] == 0) {
        RemoveUnused(*it);
        it = std::list<int>::reverse_iterator(lsInts.erase(--it.base()));
      } else {
        it++;
      }
    }
  }
  
  /* }}} Modify network end */

  /* {{{ Save & load */

  int AndNetwork::Save(int slot) {
    Action action;
    action.type = SAVE;
    if(slot < 0) {
      slot = vBackups.size();
      vBackups.push_back(*this);
    } else {
      assert(slot < vBackups.size());
      vBackups[slot] = *this;
    }
    action.id = slot;
    TakenAction(action);
    return slot;
  }

  void AndNetwork::Load(int slot) {
    assert(slot < vBackups.size());
    Action action;
    action.type = LOAD;
    action.id = slot;
    *this = vBackups[slot];
    TakenAction(action);
  }

  void AndNetwork::PopBack() {
    assert(!vBackups.empty());
    Action action;
    action.type = POP_BACK;
    action.id = vBackups.size() - 1;
    vBackups.pop_back();
    TakenAction(action);
  }

  /* }}} Save & load end */
  
  /* {{{ Misc */

  void AndNetwork::Print() const {
    std::cout << "pi: ";
    std::string delim = "";
    ForEachPi([&](int id) {
      std::cout << delim << id;
      delim = ", ";
    });
    std::cout << std::endl;
    ForEachInt([&](int id) {
      std::cout << "node " << id << ": ";
      delim = "";
      ForEachFanin(id, [&](int fi, bool c) {
        std::cout << delim;
        if(c) {
          std::cout << "!";
        }
        std::cout << fi;
        delim = ", ";
      });
      std::cout << " (ref = " << vRefs[id] << ")";
      std::cout << std::endl;
    });
    std::cout << "po: ";
    delim = "";
    ForEachPoDriver([&](int fi, bool c) {
      std::cout << delim;
      if(c) {
        std::cout << "!";
      }
      std::cout << fi;
      delim = ", ";
    });
    std::cout << std::endl;
  }

  /* }}} Misc end */

}
