#pragma once

#include <queue>
#include <random>

#ifdef ABC_USE_PTHREADS
#include <thread>
#include <mutex>
#include <condition_variable>
#endif

// #include "misc/rrrParameter.h"
// #include "misc/rrrUtils.h"
// #include "rrrAbc.h"
#include "io/rrrBinary.h"
#include "io/rrrAig.h"
#include "extra/rrrTable.h"
#include "extra/rrrEvictTable.h"
#include "extra/rrrCanonicalizer.h"

namespace rrr {

  template <typename Ntk, typename Opt, typename Par>
  class UrScheduler {
  private:
    static constexpr bool fNoIncrease = true;

    // job
    struct Job;
    
    // pointer to network
    Ntk *pOriginal;

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
    seconds nTimeout;
    std::string strTemporary;
    std::function<double(Ntk *)> CostFunction;
    static constexpr bool fTwoArgSym = false;
    
    // data
    std::vector<std::unique_ptr<Table<std::vector<int>>>> tabs;
    int nCreatedJobs;
    int nFinishedJobs;
    //std::queue<Job *> qPendingJobs;
    std::vector<std::queue<Job *>> vqPendingJobs;
    Opt *pOpt; // used only in case of single thread execution
#ifdef ABC_USE_PTHREADS
    bool fTerminate;
    std::vector<std::thread> vThreads;
    std::vector<std::unique_ptr<std::mutex>> mutexTables;
    std::mutex mutexPendingJobs;
    std::mutex mutexFinishedJobs;
    std::mutex mutexPrint;
    std::condition_variable condPendingJobs;
    std::condition_variable condFinishedJobs;
#endif

    // print
    template <typename... Args>
    void Print(int nVerboseLevel, std::string prefix, Args... args);

    // table
    bool Register(Ntk *pNtk, int src_tab, int src_idx, std::vector<Action> const &vActions, int next_tab, int &index);
    
    // run jobs
    void RunJob(Opt &opt, Job *pJob);

    // manage jobs
    Job *CreateJob(int src_tab, int src_idx, int cost, int nAdd);
    void Wait();

    // thread
#ifdef ABC_USE_PTHREADS
    void Thread(Parameter const *pPar);
#endif

  public:
    // constructors
    UrScheduler(Ntk *pNtk, Parameter const *pPar);
    ~UrScheduler();
    
    // run
    std::vector<Ntk *> Run();
  };

  /* {{{ Job */
  
  template <typename Ntk, typename Opt, typename Par>
  struct UrScheduler<Ntk, Opt, Par>::Job {
    // data
    int id;
    int src_tab;
    int src_idx;
    int cost;
    int nAdd;
    std::string prefix;
    
    // constructor
    Job(int id, int src_tab, int src_idx, int cost, int nAdd) :
      id(id),
      src_tab(src_tab),
      src_idx(src_idx),
      cost(cost),
      nAdd(nAdd) {
      std::stringstream ss;
      PrintNext(ss, "job", id, ":");
      prefix = ss.str() + " ";
    }
  };
  
  /* }}} */

  /* {{{ Print */
  
  template <typename Ntk, typename Opt, typename Par>
  template<typename... Args>
  inline void UrScheduler<Ntk, Opt, Par>::Print(int nVerboseLevel, std::string prefix, Args... args) {
    if(nVerbose <= nVerboseLevel) {
      return;
    }
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexPrint);
        std::cout << prefix;
        PrintNext(std::cout, args...);
        std::cout << std::endl;
      }
      return;
    }
#endif
    std::cout << prefix;
    PrintNext(std::cout, args...);
    std::cout << std::endl;
  }

  /* }}} */
    
  /* {{{ Table */

  template <typename Ntk, typename Opt, typename Par>
  bool UrScheduler<Ntk, Opt, Par>::Register(Ntk *pNtk, int src_tab, int src_idx, std::vector<Action> const &vActions, int next_tab, int &index) {
    std::string str, str_sym;
    {
      Ntk ntk;
      ntk.Read(*pNtk);
      Canonicalizer<Ntk> can;
      can.Run(&ntk, next_tab == 0);
      str = CreateBinary(&ntk);
      if(fTwoArgSym) {
        std::vector<int> vOrder;
        for(int i = 0; i < ntk.GetNumPis() / 2; i++) {
          vOrder.push_back(i + ntk.GetNumPis() / 2);
        }
        for(int i = 0; i < ntk.GetNumPis() / 2; i++) {
          vOrder.push_back(i);
        }        
        ntk.ChangePiOrder(vOrder);
        can.Run(&ntk, next_tab == 0);
        str_sym = CreateBinary(&ntk);
      }
    }
    std::vector<int> his;
    {
      his.push_back(src_tab);
      his.push_back(src_idx);
      for(auto const &action: vActions) {
        if(action.type == REMOVE_FANIN) {
          his.push_back(action.id);
          his.push_back(action.idx);
        } else if(action.type == ADD_FANIN) {
          his.push_back(-action.id);
          his.push_back(action.fi);
        }
      }
    }
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(*mutexTables[next_tab]);
        return tabs[next_tab]->Register(str, his, index, str_sym);
      }
    }
#endif
    return tabs[next_tab]->Register(str, his, index, str_sym);
  }
  
  /* }}} */
    
  /* {{{ Run jobs */

  template <typename Ntk, typename Opt, typename Par>
  void UrScheduler<Ntk, Opt, Par>::RunJob(Opt &opt, Job *pJob) {
    Ntk ntk;
    ntk.Read(tabs[pJob->src_tab]->Get(pJob->src_idx), BinaryReader<Ntk>);
    opt.AssignNetwork(&ntk, pJob->nAdd < nJobs);
    opt.SetPrintLine([&](std::string str) {
      Print(-1, pJob->prefix, str);
    });
    for(int k = 0; true; k++) {
      auto vActions = opt.Run();
      if(vActions.empty()) {
        if(k == 0 && pJob->nAdd == nJobs) {
          if(!fNoIncrease || CostFunction(&ntk) <= pJob->cost) {
            int index;
            bool fNew = Register(&ntk, pJob->src_tab, pJob->src_idx, vActions, 0, index);
            if(fNew) {
              std::vector<int> rems;
              for(auto const &qPendingJobs: vqPendingJobs) {
                rems.push_back(qPendingJobs.size());
              }
              Print(0, pJob->prefix, "src_tab", "=", pJob->src_tab, ",", "src_idx", "=", pJob->src_idx, "cost", "=", CostFunction(&ntk), ",", "idx", "=", index, "(new)", ",", "remaining", "=", rems);
              assert(index < 200100);
              CreateJob(0, index, CostFunction(&ntk), 0);
            }
          }
        }
        break;
      }
      int index;
      int next_tab = std::min(pJob->nAdd + 1, nJobs);
      bool fNew = Register(&ntk, pJob->src_tab, pJob->src_idx, vActions, next_tab, index);
      if(fNew) {
        Print(1, pJob->prefix, "src_tab", "=", pJob->src_tab, ",", "src_idx", "=", pJob->src_idx, "cost", "=", CostFunction(&ntk), ",", "tab", "=", next_tab, "idx", "=", index);
        CreateJob(next_tab, index, std::min(pJob->cost, (int)CostFunction(&ntk)), next_tab);
      }
    }
    tabs[pJob->src_tab]->Deref(pJob->src_idx);
    delete pJob;
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexFinishedJobs);
        if(nFinishedJobs % 1000000 == 0) {
          std::vector<int> rems, pops, sizes;
          for(auto const &qPendingJobs: vqPendingJobs) {
            rems.push_back(qPendingJobs.size());
          }
          for(int i = 0; i < nJobs + 1; i++) {
            pops.push_back(tabs[i]->GetPopulation());
            sizes.push_back(tabs[i]->Size());
          }
          Print(0, "", "finished", nFinishedJobs, "jobs", ":", "remaining", "=", rems, ",", "population", "=", pops, ",", "size", "=", sizes);
        }
        nFinishedJobs++;
        condFinishedJobs.notify_one();
      }
      return;
    }
#endif
    nFinishedJobs++;
  }

  /* }}} */

  /* {{{ Manage jobs */

  template <typename Ntk, typename Opt, typename Par>
  typename UrScheduler<Ntk, Opt, Par>::Job *UrScheduler<Ntk, Opt, Par>::CreateJob(int src_tab, int src_idx, int cost, int nAdd) {
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      Job *pJob;
      {
        std::unique_lock<std::mutex> l(mutexPendingJobs);
        pJob = new Job(nCreatedJobs++, src_tab, src_idx, cost, nAdd);
        vqPendingJobs[nJobs - src_tab].push(pJob);
        condPendingJobs.notify_one();
      }
      return pJob;
    }
#endif
    Job *pJob = new Job(nCreatedJobs++, src_tab, src_idx, cost, nAdd);
    vqPendingJobs[nJobs - src_tab].push(pJob);
    return pJob;
  }
  
  template <typename Ntk, typename Opt, typename Par>
  void UrScheduler<Ntk, Opt, Par>::Wait() {
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      while(true) {
        int nFinishedJobs_;
        {
          std::unique_lock<std::mutex> l(mutexFinishedJobs);
          nFinishedJobs_ = nFinishedJobs;
        }
        int nCreatedJobs_;
        {
          std::unique_lock<std::mutex> l(mutexPendingJobs);
          if(nCreatedJobs == nFinishedJobs) {
            break;
          }
          nCreatedJobs_ = nCreatedJobs;
        }
        {
          std::unique_lock<std::mutex> l(mutexFinishedJobs);
          while(nCreatedJobs_ > nFinishedJobs) {
            condFinishedJobs.wait(l);
          }
        }
      }
      return;
    }
#endif
    while(nCreatedJobs > nFinishedJobs) {
      Job *pJob = NULL;
      for(auto &qPendingJobs: vqPendingJobs) {
        if(!qPendingJobs.empty()) {
          pJob = qPendingJobs.front();
          qPendingJobs.pop();
          break;
        }
      }
      assert(pJob != NULL);
      RunJob(*pOpt, pJob);
    }
  }
  
  /* }}} */

  /* {{{ Thread */

#ifdef ABC_USE_PTHREADS
  template <typename Ntk, typename Opt, typename Par>
  void UrScheduler<Ntk, Opt, Par>::Thread(Parameter const *pPar) {
    Opt opt(pPar);
    while(true) {
      Job *pJob = NULL;
      {
        std::unique_lock<std::mutex> l(mutexPendingJobs);
        while(!fTerminate) {
          bool fFound = false;
          for(auto &qPendingJobs: vqPendingJobs) {
            if(!qPendingJobs.empty()) {
              fFound = true;
              break;
            }
          }
          if(fFound) {
            break;
          }
          condPendingJobs.wait(l);
        }
        if(fTerminate) {
          for(auto &qPendingJobs: vqPendingJobs) {
            assert(qPendingJobs.empty());
          }
          return;
        }
        pJob = NULL;
        for(auto &qPendingJobs: vqPendingJobs) {
          if(!qPendingJobs.empty()) {
            pJob = qPendingJobs.front();
            qPendingJobs.pop();
            break;
          }
        }
      }
      assert(pJob != NULL);
      RunJob(opt, pJob);
    }
  }
#endif
  
  /* }}} */
  
  /* {{{ Constructors */

  template <typename Ntk, typename Opt, typename Par>
  UrScheduler<Ntk, Opt, Par>::UrScheduler(Ntk *pNtk, Parameter const *pPar) :
    pOriginal(pNtk),
    nVerbose(pPar->nSchedulerVerbose),
    iSeed(pPar->iSeed),
    nFlow(pPar->nSchedulerFlow),
    nJobs(pPar->nJobs),
    fMultiThreading(pPar->nThreads > 1),
    fPartitioning(pPar->nPartitionSize > 0),
    fDeterministic(pPar->fDeterministic),
    nParallelPartitions(pPar->nParallelPartitions),
    fOptOnInsert(pPar->fOptOnInsert),
    nTimeout(pPar->nTimeout),
    strTemporary(pPar->strTemporary),
    nCreatedJobs(0),
    nFinishedJobs(0) {
    CostFunction = [](Ntk *pNtk) {
      int nTwoInputSize = 0;
      pNtk->ForEachInt([&](int id) {
        nTwoInputSize += pNtk->GetNumFanins(id) - 1;
      });
      return nTwoInputSize;
    };
    vqPendingJobs.resize(nJobs + 1);
    tabs.emplace_back(std::make_unique<Table<std::vector<int>>>(20));
    for(int i = 0; i < nJobs; i++) {
      tabs.emplace_back(std::make_unique<EvictTable<std::vector<int>>>(20, 25));
    }
    pOpt = new Opt(pPar);
#ifdef ABC_USE_PTHREADS
    for(int i = 0; i < nJobs + 1; i++) {
      mutexTables.emplace_back(std::make_unique<std::mutex>());
    }
    fTerminate = false;
    if(fMultiThreading) {
      vThreads.reserve(pPar->nThreads);
      for(int i = 0; i < pPar->nThreads; i++) {
        vThreads.emplace_back(std::bind(&UrScheduler::Thread, this, pPar));
      }
    }
#endif
  }

  template <typename Ntk, typename Opt, typename Par>
  UrScheduler<Ntk, Opt, Par>::~UrScheduler() {
    delete pOpt;
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexPendingJobs);
        fTerminate = true;
        condPendingJobs.notify_all();
      }
      for(std::thread &t: vThreads) {
        t.join();
      }
    }
#endif
  }

  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Opt, typename Par>
  std::vector<Ntk *> UrScheduler<Ntk, Opt, Par>::Run() {
    pOriginal->Sweep(true);
    pOriginal->TrivialCollapse();
    bool fRedundant = pOpt->IsRedundant(pOriginal);
    std::vector<Action> vActions;
    int index;
    Register(pOriginal, -1, 0, vActions, 0, index);
    assert(!fRedundant);
    CreateJob(0, index, CostFunction(pOriginal), 0);

    // wait until all jobs are done
    Wait();

    Print(-1, "","unique", "=", tabs[0]->Size() - (int)fRedundant);
    Print(-1, "","jobs", "=", nFinishedJobs);

    std::vector<Ntk *> vNtks;
    for(int i = 0; i < tabs[0]->Size(); i++) {
      std::string str = tabs[0]->Get(i);
      Ntk *pNtk = new Ntk;
      pNtk->Read(str, BinaryReader<Ntk>);
      vNtks.push_back(pNtk);
    }
    return vNtks;
  }

  /* }}} */

}
