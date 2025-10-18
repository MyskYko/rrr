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
  class CsoScheduler {
  private:
    // aliases
    static constexpr char *pCompress2rs = "balance -l; resub -K 6 -l; rewrite -l; resub -K 6 -N 2 -l; refactor -l; resub -K 8 -l; balance -l; resub -K 8 -N 2 -l; rewrite -l; resub -K 10 -l; rewrite -z -l; resub -K 10 -N 2 -l; balance -l; resub -K 12 -l; refactor -z -l; resub -K 12 -N 2 -l; rewrite -z -l; balance -l";
    
    // pointer to network
    Ntk *pNtk;

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
    Opt *pOpt;
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
    CsoScheduler(Ntk *pNtk, Parameter const *pPar);
    ~CsoScheduler();
    
    // run
    void Run();
  };

  /* {{{ Print */
  
  template <typename Ntk, typename Opt, typename Par>
  template<typename... Args>
  inline void CsoScheduler<Ntk, Opt, Par>::Print(int nVerboseLevel, std::string prefix, Args... args) {
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
  inline seconds CsoScheduler<Ntk, Opt, Par>::GetRemainingTime() const {
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
  inline double CsoScheduler<Ntk, Opt, Par>::GetElapsedTime() const {
    time_point timeCurrent = GetCurrentTime();
    return Duration(timeStart, timeCurrent);
  }

  /* }}} */
  
  /* {{{ Summary */

  template <typename Ntk, typename Opt, typename Par>
  template <typename T>
  void CsoScheduler<Ntk, Opt, Par>::AddToSummary(std::vector<std::string> &keys, std::map<std::string, T> &m, summary<T> const &result) const {
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
  CsoScheduler<Ntk, Opt, Par>::CsoScheduler(Ntk *pNtk, Parameter const *pPar) :
    pNtk(pNtk),
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
    pOpt = new Opt(pPar, CostFunction);
  }

  template <typename Ntk, typename Opt, typename Par>
  CsoScheduler<Ntk, Opt, Par>::~CsoScheduler() {
    delete pOpt;
  }

  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Opt, typename Par>
  void CsoScheduler<Ntk, Opt, Par>::Run() {
    timeStart = GetCurrentTime();
    double costStart = CostFunction(pNtk);

    pOpt->AssignNetwork(pNtk);
    pOpt->SetPrintLine([&](std::string str) {
      Print(-1, "", str);
    });

    double cost = costStart;

    // first round
    Print(0, "", "round", 0, ":", "cost", "=", cost);
    pOpt->Run();
    cost = CostFunction(pNtk);

    // iterative threshold increase
    for(int k = 1; cost > 0; k++) {
      Print(0, "", "round", k, ":", "cost", "=", cost, "next threshold", "=", pOpt->GetNext(), "elapsed", "=", GetElapsedTime(), "s");
      if(fRelaxOnRemoval) {
        Print(0, "", "increasing delta from", pOpt->GetDelta(), "by", pOpt->GetNext() - pOpt->GetThreshold());
        pOpt->SetDelta(pOpt->GetDelta() + pOpt->GetNext() - pOpt->GetThreshold());
        // Note: separating set delta and set threshold is good because it allows to set delta to other values than delta + next - current
      }
      pOpt->SetThreshold(pOpt->GetNext());
      bool fReduced = pOpt->Run();
      assert(fReduced);
      cost = CostFunction(pNtk);
    }

    AddToSummary(vStatsSummaryKeys, mStatsSummary, pOpt->GetStatsSummary());
    AddToSummary(vTimesSummaryKeys, mTimesSummary, pOpt->GetTimesSummary());
    pOpt->ResetSummary();
    
    cost = CostFunction(pNtk);
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
