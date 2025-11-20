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
  class RotatingThresholdScheduler {
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
    int nRelaxOnRemoval;
    bool fNoRelax;
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
    RotatingThresholdScheduler(std::vector<Ntk *> vNtks, Parameter const *pPar);
    ~RotatingThresholdScheduler();
    
    // run
    void Run();
  };

  /* {{{ Print */
  
  template <typename Ntk, typename Opt, typename Par>
  template<typename... Args>
  inline void RotatingThresholdScheduler<Ntk, Opt, Par>::Print(int nVerboseLevel, std::string prefix, Args... args) {
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
  inline seconds RotatingThresholdScheduler<Ntk, Opt, Par>::GetRemainingTime() const {
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
  inline double RotatingThresholdScheduler<Ntk, Opt, Par>::GetElapsedTime() const {
    time_point timeCurrent = GetCurrentTime();
    return Duration(timeStart, timeCurrent);
  }

  /* }}} */
  
  /* {{{ Summary */

  template <typename Ntk, typename Opt, typename Par>
  template <typename T>
  void RotatingThresholdScheduler<Ntk, Opt, Par>::AddToSummary(std::vector<std::string> &keys, std::map<std::string, T> &m, summary<T> const &result) const {
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
  RotatingThresholdScheduler<Ntk, Opt, Par>::RotatingThresholdScheduler(std::vector<Ntk *> vNtks, Parameter const *pPar) :
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
    nRelaxOnRemoval(pPar->nRelaxOnRemoval),
    fNoRelax(pPar->fNoRelax),
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
  RotatingThresholdScheduler<Ntk, Opt, Par>::~RotatingThresholdScheduler() {
    for(int i = 0; i < int_size(vOpts); i++) {
      delete vOpts[i];
    }
  }

  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Opt, typename Par>
  void RotatingThresholdScheduler<Ntk, Opt, Par>::Run() {
    timeStart = GetCurrentTime();
    double costStart = 0;
    for(Ntk *pNtk: vNtks) {
      costStart += CostFunction(pNtk);
    }

    for(int i = 0; i < int_size(vNtks); i++) {
      vOpts[i]->AssignNetwork(vNtks[i], i);
      vOpts[i]->SetNumModule(i);
    }

    // set default threshold
    for(int i = 0; i < int_size(vNtks); i++) {
      std::vector<std::vector<int>> vBias;
      for(int j = 0; j < int_size(vNtks); j++) {
        if(i != j) {
          std::vector<std::vector<int>> vContribution = vOpts[j]->GetContribution();
          if(vBias.empty()) {
            vBias = vContribution;
          } else {
            assert(vBias.size() == vContribution.size());
            for(int idx = 0; idx < int_size(vBias); idx++) {
              assert(vBias[idx].size() == vContribution[idx].size());
              for(int k = 0; k < int_size(vBias[idx]); k++) {
                vBias[idx][k] += vContribution[idx][k];
              }
            }
          }
        }
      }
      vOpts[i]->SetNumTemporary(1); // prevent print
      vOpts[i]->SetBias(vBias);
      vOpts[i]->ResetThreshold();
    }

    // hack delta with double min so loss decrease may lower the threshold
    for(int i = 0; i < int_size(vNtks); i++) {
      vOpts[i]->SetDelta(std::numeric_limits<double>::min());
    }

    int nTemporary = 1;
    double cost = costStart;
    double tDeltaDelta = 0;
    for(int k = 0; cost > 0; k++) {
      Print(0, "", "round", k, ":", "cost", "=", cost, "elapsed", "=", GetElapsedTime(), "s");
      bool fReduced = false;
      for(int i = 0; i < int_size(vNtks); i++) {
        std::vector<std::vector<int>> vBias;
        for(int j = 0; j < int_size(vNtks); j++) {
          if(i != j) {
            std::vector<std::vector<int>> vContribution = vOpts[j]->GetContribution();
            if(vBias.empty()) {
              vBias = vContribution;
            } else {
              assert(vBias.size() == vContribution.size());
              for(int idx = 0; idx < int_size(vBias); idx++) {
                assert(vBias[idx].size() == vContribution[idx].size());
                for(int k = 0; k < int_size(vBias[idx]); k++) {
                  vBias[idx][k] += vContribution[idx][k];
                }
              }
            }
          }
        }
        // Note: by sharing bias, threshold is also shared through recomputation of current + delta; otherwise (when delta = 0) all modules have the same threshold as set to next below
        vOpts[i]->SetBias(vBias);
        Print(1, "", "module", i, ":", "threshold", "=", vOpts[i]->GetThreshold(), "cost", "=", CostFunction(vNtks[i]), "elapsed", "=", GetElapsedTime(), "s");
        vOpts[i]->SetNumTemporary(nTemporary);
        vOpts[i]->SetPrintLine([&](std::string str) {
          Print(-1, "module " + std::to_string(i) + " : ", str);
        });
        fReduced |= vOpts[i]->RunOneTraversal();
        nTemporary = vOpts[i]->GetNumTemporary();
      }
      if(!fReduced) {
        if(fNoRelax) {
          break;
        }
        int idx = -1;
        double tNext = std::numeric_limits<double>::max();
        for(int i = 0; i < int_size(vNtks); i++) {
          if(tNext > vOpts[i]->GetNext()) { // TODO: this assumes we are increasing threshold
            tNext = vOpts[i]->GetNext();
            idx = i;
          }
        }
        Print(1, "", "increasing threshold to", tNext, "for module", idx);
        if(nRelaxOnRemoval) {
          if(nRelaxOnRemoval == 3) {
            if(tDeltaDelta == 0) {
              tDeltaDelta = vOpts[0]->GetThreshold() / cost;
            }
            double tDelta = tDeltaDelta;
            if(tDelta < tNext - vOpts[0]->GetThreshold()) {
              tDelta = tNext - vOpts[0]->GetThreshold();
            }
            Print(0, "", "increasing delta from", vOpts[0]->GetDelta(), "by", tDelta);
            tDelta += vOpts[0]->GetDelta();
            for(int i = 0; i < int_size(vNtks); i++) {
              vOpts[i]->SetDelta(tDelta);
            }
          } else if(nRelaxOnRemoval == 2 || (nRelaxOnRemoval == 1 && vOpts[0]->GetDelta() == std::numeric_limits<double>::min())) {
            double tDelta = vOpts[0]->GetThreshold() / cost;
            if(tDelta < tNext - vOpts[0]->GetThreshold()) {
              tDelta = tNext - vOpts[0]->GetThreshold();
            }
            Print(0, "", "increasing delta from", vOpts[0]->GetDelta(), "by", tDelta);
            tDelta += vOpts[0]->GetDelta();
            for(int i = 0; i < int_size(vNtks); i++) {
              vOpts[i]->SetDelta(tDelta);
            }
          } else {
            Print(0, "", "increasing delta from", vOpts[0]->GetDelta(), "by", tNext - vOpts[0]->GetThreshold());
            double tDelta = vOpts[0]->GetDelta() + tNext - vOpts[0]->GetThreshold();
            for(int i = 0; i < int_size(vNtks); i++) {
              assert(vOpts[i]->GetDelta() + tNext - vOpts[i]->GetThreshold() == tDelta);
              vOpts[i]->SetDelta(tDelta);
            }
          }
        }
        for(int i = 0; i < int_size(vNtks); i++) {
          vOpts[i]->SetThreshold(tNext);
        }
        vOpts[idx]->SetPrintLine([&](std::string str) {
          Print(-1, "module " + std::to_string(idx) + " : ", str);
        });
        vOpts[idx]->RemoveNext();
        nTemporary = vOpts[idx]->GetNumTemporary();
      }
      cost = 0;
      for(Ntk *pNtk: vNtks) {
        cost += CostFunction(pNtk);
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
