#pragma once

#include <map>
#include <queue>
#include <random>

#ifdef ABC_USE_PTHREADS
#include <thread>
#include <mutex>
#include <condition_variable>
#endif

#include "misc/rrrParameter.h"
#include "misc/rrrUtils.h"
#include "interface/rrrAbc.h"

namespace rrr {

  template <typename Ntk, typename Opt, typename Par>
  class DlsScheduler {
  private:
    // aliases
    static constexpr char *pCompress2rs = "balance -l; resub -K 6 -l; rewrite -l; resub -K 6 -N 2 -l; refactor -l; resub -K 8 -l; balance -l; resub -K 8 -N 2 -l; rewrite -l; resub -K 10 -l; rewrite -z -l; resub -K 10 -N 2 -l; balance -l; resub -K 12 -l; refactor -z -l; resub -K 12 -N 2 -l; rewrite -z -l; balance -l";
    
    // pointer to network
    std::vector<Ntk *> vNtks;

    // parameters
    int nVerbose;
    int iSeed;
    int nFlow;
    int nJobs;
    bool fMultiThreading;
    bool fPartitioning;
    bool fDeterministic;
    int nParallelPartitions;
    bool fOptOnInsert;
    bool fRelaxOnRemoval;
    seconds nTimeout;
    std::function<double(Ntk *)> CostFunction;
    
    // data
    int nCreatedJobs;
    int nFinishedJobs;
    time_point timeStart;
    Par par;
    std::vector<Opt *> vOpts;
    std::vector<std::string> vStatsSummaryKeys;
    std::map<std::string, int> mStatsSummary;
    std::vector<std::string> vTimesSummaryKeys;
    std::map<std::string, double> mTimesSummary;

    // print
    template <typename... Args>
    void Print(int nVerboseLevel, std::string prefix, Args... args);
    
    // time
    seconds GetRemainingTime() const;
    double GetElapsedTime() const;
    
    // summary
    template <typename T>
    void AddToSummary(std::vector<std::string> &keys, std::map<std::string, T> &m, summary<T> const &result) const;

  public:
    // constructors
    DlsScheduler(std::vector<Ntk *> vNtks, Parameter const *pPar);
    ~DlsScheduler();
    
    // run
    void Run();
  };

  /* {{{ Print */
  
  template <typename Ntk, typename Opt, typename Par>
  template<typename... Args>
  inline void DlsScheduler<Ntk, Opt, Par>::Print(int nVerboseLevel, std::string prefix, Args... args) {
    if(nVerbose <= nVerboseLevel) {
      return;
    }
    std::cout << prefix;
    PrintNext(std::cout, args...);
    std::cout << std::endl;
  }

  /* }}} */
  
  /* {{{ Time */

  template <typename Ntk, typename Opt, typename Par>
  inline seconds DlsScheduler<Ntk, Opt, Par>::GetRemainingTime() const {
    if(nTimeout == 0) {
      return 0;
    }
    time_point timeCurrent = GetCurrentTime();
    seconds nRemainingTime = nTimeout - DurationInSeconds(timeStart, timeCurrent);
    if(nRemainingTime == 0) { // avoid glitch
      return -1;
    }
    return nRemainingTime;
  }

  template <typename Ntk, typename Opt, typename Par>
  inline double DlsScheduler<Ntk, Opt, Par>::GetElapsedTime() const {
    time_point timeCurrent = GetCurrentTime();
    return Duration(timeStart, timeCurrent);
  }

  /* }}} */
  
  /* {{{ Summary */

  template <typename Ntk, typename Opt, typename Par>
  template <typename T>
  void DlsScheduler<Ntk, Opt, Par>::AddToSummary(std::vector<std::string> &keys, std::map<std::string, T> &m, summary<T> const &result) const {
    std::vector<std::string>::iterator it = keys.begin();
    for(auto const &entry: result) {
      if(m.count(entry.first)) {
        m[entry.first] += entry.second;
        it = std::find(it, keys.end(), entry.first);
        assert(it != keys.end());
        it++;
      } else {
        m[entry.first] = entry.second;
        it = keys.insert(it, entry.first);
        it++;          
      }
    }
  }
  
  /* }}} */

  /* {{{ Constructors */

  template <typename Ntk, typename Opt, typename Par>
  DlsScheduler<Ntk, Opt, Par>::DlsScheduler(std::vector<Ntk *> vNtks, Parameter const *pPar) :
    vNtks(vNtks),
    nVerbose(pPar->nSchedulerVerbose),
    iSeed(pPar->iSeed),
    nFlow(pPar->nSchedulerFlow),
    nJobs(pPar->nJobs),
    fMultiThreading(pPar->nThreads > 1),
    fPartitioning(pPar->nPartitionSize > 0),
    fDeterministic(pPar->fDeterministic),
    nParallelPartitions(pPar->nParallelPartitions),
    fOptOnInsert(pPar->fOptOnInsert),
    fRelaxOnRemoval(pPar->fRelaxOnRemoval),
    nTimeout(pPar->nTimeout),
    nCreatedJobs(0),
    nFinishedJobs(0),
    par(pPar) {
    // prepare cost function
    CostFunction = [](Ntk *pNtk) {
      int nTwoInputSize = 0;
      pNtk->ForEachInt([&](int id) {
        nTwoInputSize += pNtk->GetNumFanins(id) - 1;
      });
      return nTwoInputSize;
    };
    assert(!fMultiThreading);
    for(int i = 0; i < int_size(vNtks); i++) {
      vOpts.push_back(new Opt(pPar, CostFunction));
    }
  }

  template <typename Ntk, typename Opt, typename Par>
  DlsScheduler<Ntk, Opt, Par>::~DlsScheduler() {
    for(int i = 0; i < int_size(vOpts); i++) {
      delete vOpts[i];
    }
  }

  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Opt, typename Par>
  void DlsScheduler<Ntk, Opt, Par>::Run() {
    timeStart = GetCurrentTime();
    double costStart = 0;
    for(Ntk *pNtk: vNtks) {
      costStart += CostFunction(pNtk);
    }

    for(int i = 0; i < int_size(vNtks); i++) {
      vOpts[i]->AssignNetwork(vNtks[i], i);
    }

    int nTemporary = 0;
    double cost = costStart;
    double dDelta = 0;
    for(int k = 0; cost > 0; k++) {
      Print(0, "round", k, ":", "cost", "=", cost);
      bool fReduced = false;
      for(int i = 0; i < int_size(vNtks); i++) {
        Print(1, "module", i);
        std::vector<std::vector<int>> vBias;
        for(int j = 0; j < int_size(vNtks); j++) {
          if(i != j) {
            auto vOutputs = vOpts[j]->GetOutputs();
            if(vBias.empty()) {
              vBias = vOutputs;
            } else {
              assert(0); // take sum
            }
          }
        }
        vOpts[i]->SetBias(vBias);
        vOpts[i]->SetNumTemporary(nTemporary);
        vOpts[i]->SetThreshold(vOpts[i]->GetLoss() + dDelta);
        vOpts[i]->SetPrintLine([&](std::string str) {
          Print(-1, "module " + std::to_string(i) + " : ", str);
        });
        fReduced |= vOpts[i]->Run();
        nTemporary = vOpts[i]->GetNumTemporary();
      }
      if(!fReduced) {
        int idx = -1;
        double dMinimum = std::numeric_limits<double>::max();
        for(int i = 0; i < int_size(vNtks); i++) {
          if(dMinimum > vOpts[i]->GetMinimum()) {
            dMinimum = vOpts[i]->GetMinimum();
            idx = i;
          }
        }
        Print(1, "increasing threshold to", dMinimum, "for module", idx);
        vOpts[idx]->SetPrintLine([&](std::string str) {
          Print(-1, "module " + std::to_string(idx) + " : ", str);
        });
        vOpts[idx]->RemoveMinimum();
        nTemporary = vOpts[idx]->GetNumTemporary();
        cost = 0;
        for(Ntk *pNtk: vNtks) {
          cost += CostFunction(pNtk);
        }
        if(fRelaxOnRemoval && dDelta == 0) {
          dDelta = vOpts[idx]->GetLoss() / cost;
          Print(1, "", "setting delta to", dDelta);
          for(int i = 0; i < int_size(vNtks); i++) {
            vOpts[i]->SetDelta(dDelta);
          }
        }
      } else {
        cost = 0;
        for(Ntk *pNtk: vNtks) {
          cost += CostFunction(pNtk);
        }
      }
    }

    for(int i = 0; i < int_size(vNtks); i++) {
      AddToSummary(vStatsSummaryKeys, mStatsSummary, vOpts[i]->GetStatsSummary());
      AddToSummary(vTimesSummaryKeys, mTimesSummary, vOpts[i]->GetTimesSummary());
      vOpts[i]->ResetSummary();
    }
    
    cost = 0;
    for(Ntk *pNtk: vNtks) {
      cost += CostFunction(pNtk);
    }
    double duration = GetElapsedTime();
    Print(0, "\n", "stats summary", ":");
    for(std::string key: vStatsSummaryKeys) {
      Print(0, "\t", SW{30, true}, key, ":", SW{10}, mStatsSummary[key]);
    }
    Print(0, "", "runtime summary", ":");
    for(std::string key: vTimesSummaryKeys) {
      Print(0, "\t", SW{30, true}, key, ":", mTimesSummary[key], "s", "(", 100 * mTimesSummary[key] / duration, "%", ")");
    }
    Print(0, "", "end", ":", "cost", "=", cost, "(", 100 * (cost - costStart) / costStart, "%", ")", ",", "time", "=", duration, "s");
  }

  /* }}} */

}
