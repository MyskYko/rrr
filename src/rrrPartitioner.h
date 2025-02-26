#pragma once

#include <vector>
#include <set>
#include <map>

#include "rrrParameter.h"
#include "rrrUtils.h"

namespace rrr {

  template <typename Ntk>
  class Partitioner {
  private:
    // pointer to network
    Ntk *pNtk;

    // parameters
    int nVerbose;
    static constexpr bool fExcludeLoops = false;

    // data
    std::map<Ntk *, std::pair<std::vector<int>, std::vector<int>>> mSubNtk2Io;
    
  public:
    // constructors
    Partitioner(Parameter const *pPar);
    void UpdateNetwork(Ntk *pNtk);

    Ntk *Extract();
    void Insert(Ntk *pSubNtk);
  };

  /* {{{ Constructors */
  
  template <typename Ntk>
  Partitioner<Ntk>::Partitioner(Parameter const *pPar) :
    nVerbose(pPar->nSchedulerVerbose) {
  }

  template <typename Ntk>
  void Partitioner<Ntk>::UpdateNetwork(Ntk *pNtk_) {
    pNtk = pNtk_;
  }

  /* }}} */

  template <typename Ntk>
  Ntk *Partitioner<Ntk>::Extract() {
    // TODO: pick a center node from candidates that do not belong to any other ongoing windows (for example, set of nodes as sBlocked)
    int id = 100;
    // collect neighbor
    std::vector<int> vNodes = pNtk->GetNeighbors(id, false, 2);
    std::set<int> sNodes(vNodes.begin(), vNodes.end());
    sNodes.insert(id);
    if(nVerbose) {
      std::cout << "neighbors: " << sNodes << std::endl;
    }
    // TODO: remove nodes that are already blocked
    // get tentative window IO
    std::set<int> sInputs, sOutputs;
    for(int id: sNodes) {
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        if(!sNodes.count(fi)) {
          sInputs.insert(fi);
        }
      });
      bool fOutput = false;
      pNtk->ForEachFanout(id, true, [&](int fo, bool c) {
        if(!sNodes.count(fo)) {
          fOutput = true;
        }
      });
      if(fOutput) {
        sOutputs.insert(id);
      }
    }
    if(nVerbose) {
      std::cout << "\tinputs: " << sInputs << std::endl;
      std::cout << "\toutputs: " << sOutputs << std::endl;
    }
    // prevent potential loops
    if(!fExcludeLoops) {
      // by including inner nodes
      std::set<int> sFanouts;
      for(int id: sOutputs) {
        pNtk->ForEachFanout(id, false, [&](int fo, bool c) {
          if(!sNodes.count(fo)) {
            sFanouts.insert(fo);
          }
        });
      }
      std::vector<int> vInners = pNtk->GetInners(sFanouts, sInputs);
      while(!vInners.empty()) {
        if(nVerbose) {
          std::cout << "inners: " << vInners << std::endl;
        }
        sNodes.insert(vInners.begin(), vInners.end());
        if(nVerbose) {
          std::cout << "new neighbors: " << sNodes << std::endl;
        }
        sInputs.clear();
        sOutputs.clear();
        for(int id: sNodes) {
          pNtk->ForEachFanin(id, [&](int fi, bool c) {
            if(!sNodes.count(fi)) {
              sInputs.insert(fi);
            }
          });
          bool fOutput = false;
          pNtk->ForEachFanout(id, true, [&](int fo, bool c) {
            if(!sNodes.count(fo)) {
              fOutput = true;
            }
          });
          if(fOutput) {
            sOutputs.insert(id);
          }
        }
        if(nVerbose) {
          std::cout << "\tnew inputs: " << sInputs << std::endl;
          std::cout << "\tnew outputs: " << sOutputs << std::endl;
        }
        sFanouts.clear();;
        for(int id: sOutputs) {
          pNtk->ForEachFanout(id, false, [&](int fo, bool c) {
            if(!sNodes.count(fo)) {
              sFanouts.insert(fo);
            }
          });
        }
        vInners = pNtk->GetInners(sFanouts, sInputs);
      }
    } else {
      // by removing TFI of outputs reachable to inputs
      for(int id: sOutputs) {
        if(!sNodes.count(id)) { // already removed
          continue;
        }
        std::set<int> sFanouts;
        pNtk->ForEachFanout(id, false, [&](int fo, bool c) {
          if(!sNodes.count(fo)) {
            sFanouts.insert(fo);
          }
        });
        if(pNtk->IsReachable(sFanouts, sInputs)) {
          if(nVerbose) {
            std::cout << id << " is reachable to inputs" << std::endl;
          }
          sNodes.erase(id);
          pNtk->ForEachTfiEnd(id, sInputs, [&](int fi) {
            int r = sNodes.erase(fi);
            assert(r);
          });
          if(nVerbose) {
            std::cout << "new neighbors: " << sNodes << std::endl;
          }
          // recompute inputs
          sInputs.clear();
          for(int id: sNodes) {
            pNtk->ForEachFanin(id, [&](int fi, bool c) {
              if(!sNodes.count(fi)) {
                sInputs.insert(fi);
              }
            });
          }
          if(nVerbose) {
            std::cout << "\tnew inputs: " << sInputs << std::endl;
          }
        }
      }
      // recompute outputs
      for(std::set<int>::iterator it = sOutputs.begin(); it != sOutputs.end();) {
        if(!sNodes.count(*it)) {
          it = sOutputs.erase(it);
        } else {
          it++;
        }
      }
      if(nVerbose) {
        std::cout << "\tnew outputs: " << sOutputs << std::endl;
      }
    }
    // TODO: ensure outputs of neigher windows can reach each other's inputs
    // extract by inputs, internals, and outputs (no checks needed in ntk side)
    std::vector<int> vInputs(sInputs.begin(), sInputs.end());
    std::vector<int> vOutputs(sOutputs.begin(), sOutputs.end());
    Ntk *pSubNtk = pNtk->Extract(sNodes, vInputs, vOutputs);
    // return subntk to be modified, while remember IOs
    mSubNtk2Io.emplace(pSubNtk, std::make_pair(std::move(vInputs), std::move(vOutputs)));
    return pSubNtk;
  }

  template <typename Ntk>
  void Partitioner<Ntk>::Insert(Ntk *pSubNtk) {
    pNtk->Insert(pSubNtk, std::get<0>(mSubNtk2Io[pSubNtk]), std::get<1>(mSubNtk2Io[pSubNtk]));
    delete pSubNtk;
    mSubNtk2Io.erase(pSubNtk);
  }
  
}
