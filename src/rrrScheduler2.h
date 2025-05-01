#pragma once

#include <queue>
#include <random>

#ifdef ABC_USE_PTHREADS
#include <thread>
#include <mutex>
#include <condition_variable>
#endif

#include "rrrParameter.h"
#include "rrrUtils.h"
#include "rrrAbc.h"
#include "rrrBinary.h"
#include "rrrCanonicalizer.h"

namespace rrr {

  template <typename Ntk, typename Opt, typename Par>
  class Scheduler2 {
  private:
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
    std::function<double(Ntk *)> CostFunction;
    static constexpr bool fTwoArgSym = false;
    
    // data
    int nUniques;
    std::map<std::string, int> table;
    std::vector<std::string> strs;
    std::vector<std::vector<std::pair<int, std::vector<Action>>>> history;
    int nCreatedJobs;
    int nFinishedJobs;
    std::queue<Job *> qPendingJobs;
    Opt *pOpt; // used only in case of single thread execution
#ifdef ABC_USE_PTHREADS
    bool fTerminate;
    std::vector<std::thread> vThreads;
    std::mutex mutexTable;
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
    bool Register(Ntk *pNtk, int src, std::vector<Action> const &vActions, int &index);
    
    // run jobs
    void RunJob(Opt &opt, Job *pJob);

    // manage jobs
    Job *CreateJob(int src, bool fAdd);
    void Wait();

    // thread
#ifdef ABC_USE_PTHREADS
    void Thread(Parameter const *pPar);
#endif

  public:
    // constructors
    Scheduler2(Ntk *pNtk, Parameter const *pPar);
    ~Scheduler2();
    
    // run
    void Run();
  };

  /* {{{ Job */
  
  template <typename Ntk, typename Opt, typename Par>
  struct Scheduler2<Ntk, Opt, Par>::Job {
    // data
    int id;
    int src;
    bool fAdd;
    std::string prefix;
    
    // constructor
    Job(int id, int src, bool fAdd) :
      id(id),
      src(src),
      fAdd(fAdd) {
      std::stringstream ss;
      PrintNext(ss, "job", id, ":");
      prefix = ss.str() + " ";
    }
  };
  
  /* }}} */

  /* {{{ Print */
  
  template <typename Ntk, typename Opt, typename Par>
  template<typename... Args>
  inline void Scheduler2<Ntk, Opt, Par>::Print(int nVerboseLevel, std::string prefix, Args... args) {
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
  bool Scheduler2<Ntk, Opt, Par>::Register(Ntk *pNtk, int src, std::vector<Action> const &vActions, int &index) {
    std::string str, str_sym;
    {
      Ntk ntk;
      ntk.Read(*pNtk);
      Canonicalizer<Ntk> can;
      can.Run(&ntk);
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
        can.Run(&ntk);
        str_sym = CreateBinary(&ntk);
      }
    }
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexTable);
        if(table.count(str)) {
          index = table[str];
          history[index].emplace_back(src, vActions);
          return false;
        }
        if(fTwoArgSym) {
          if(table.count(str_sym)) {
            index = table[str_sym];
            history[index].emplace_back(src, vActions);
            return false;
          }
        }
        index = nUniques++;
        table[str] = index;
        strs.push_back(str);
        if(int_size(history) < nUniques) {
          history.resize(nUniques);
        }
        history[index].emplace_back(src, vActions);
        return true;
      }
    }
#endif
    if(table.count(str)) {
      index = table[str];
      history[index].emplace_back(src, vActions);
      return false;
    }
    if(fTwoArgSym) {
      if(table.count(str_sym)) {
        index = table[str_sym];
        history[index].emplace_back(src, vActions);
        return false;
      }
    }
    index = nUniques++;
    table[str] = index;
    strs.push_back(str);
    if(int_size(history) < nUniques) {
      history.resize(nUniques);
    }
    history[index].emplace_back(src, vActions);
    return true;
  }
  
  /* }}} */
    
  /* {{{ Run jobs */

  template <typename Ntk, typename Opt, typename Par>
  void Scheduler2<Ntk, Opt, Par>::RunJob(Opt &opt, Job *pJob) {
    Ntk ntk;
    ntk.Read(strs[pJob->src], BinaryReader<Ntk>);
    opt.AssignNetwork(&ntk, pJob->fAdd);
    int nChoices = 0, nNews = 0;
    while(true) {
      auto vActions = opt.Run();
      if(vActions.empty()) {
        break;
      }
      int index;
      bool fNew = Register(&ntk, pJob->src, vActions, index);
      if(fNew) {
        Print(0, pJob->prefix, "src", "=", pJob->src, ",", "choice", "=", nChoices, "new", "=", nNews, ",", "cost", "=", CostFunction(&ntk), ",", "result", "=", index, "(new)");
        CreateJob(index, true);
        nNews++;
      } else {
        Print(0, pJob->prefix, "src", "=", pJob->src, ",", "choice", "=", nChoices, ",", "cost", "=", CostFunction(&ntk), ",", "result", "=", index);
      }
      /*
      for(auto action: vActions) {
        auto ss = GetActionDescription(action);
        std::string str;
        std::getline(ss, str);
        std::cout << str << std::endl;
      }
      std::cout << std::endl;
      */
      nChoices++;
    }
    delete pJob;
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexFinishedJobs);
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
  typename Scheduler2<Ntk, Opt, Par>::Job *Scheduler2<Ntk, Opt, Par>::CreateJob(int src, bool fAdd) {
    Job *pJob = new Job(nCreatedJobs++, src, fAdd);
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexPendingJobs);
        qPendingJobs.push(pJob);
        condPendingJobs.notify_one();
      }
      return pJob;
    }
#endif
    qPendingJobs.push(pJob);
    return pJob;
  }
  
  template <typename Ntk, typename Opt, typename Par>
  void Scheduler2<Ntk, Opt, Par>::Wait() {
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
      assert(!qPendingJobs.empty());
      Job *pJob = qPendingJobs.front();
      qPendingJobs.pop();
      RunJob(*pOpt, pJob);
    }
  }
  
  /* }}} */

  /* {{{ Thread */

#ifdef ABC_USE_PTHREADS
  template <typename Ntk, typename Opt, typename Par>
  void Scheduler2<Ntk, Opt, Par>::Thread(Parameter const *pPar) {
    Opt opt(pPar);
    while(true) {
      Job *pJob = NULL;
      {
        std::unique_lock<std::mutex> l(mutexPendingJobs);
        while(!fTerminate && qPendingJobs.empty()) {
          condPendingJobs.wait(l);
        }
        if(fTerminate) {
          assert(qPendingJobs.empty());
          return;
        }
        pJob = qPendingJobs.front();
        qPendingJobs.pop();
      }
      assert(pJob != NULL);
      RunJob(opt, pJob);
    }
  }
#endif
  
  /* }}} */
  
  /* {{{ Constructors */

  template <typename Ntk, typename Opt, typename Par>
  Scheduler2<Ntk, Opt, Par>::Scheduler2(Ntk *pNtk, Parameter const *pPar) :
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
    nUniques(0),
    nCreatedJobs(0),
    nFinishedJobs(0) {
    CostFunction = [](Ntk *pNtk) {
      int nTwoInputSize = 0;
      pNtk->ForEachInt([&](int id) {
        nTwoInputSize += pNtk->GetNumFanins(id) - 1;
      });
      return nTwoInputSize;
    };
    pOpt = new Opt(pPar);
#ifdef ABC_USE_PTHREADS
    fTerminate = false;
    if(fMultiThreading) {
      vThreads.reserve(pPar->nThreads);
      for(int i = 0; i < pPar->nThreads; i++) {
        vThreads.emplace_back(std::bind(&Scheduler2::Thread, this, pPar));
      }
    }
#endif
  }

  template <typename Ntk, typename Opt, typename Par>
  Scheduler2<Ntk, Opt, Par>::~Scheduler2() {
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
  void Scheduler2<Ntk, Opt, Par>::Run() {
    pOriginal->Sweep(true);
    pOriginal->TrivialCollapse();
    bool fRedundant = pOpt->IsRedundant(pOriginal);
    std::vector<Action> vActions;
    int index;
    Register(pOriginal, -1, vActions, index);
    CreateJob(index, !fRedundant);

    // wait until all jobs are done
    Wait();

    Print(-1, "","unique", "=", nUniques - (int)fRedundant);
    Print(-1, "","jobs", "=", nFinishedJobs);
    
    // for(std::string s: strs) {
    //   Ntk a;
    //   a.Read(s, BinaryReader<Ntk>);
    //   a.Print();
    // }
  }

  /* }}} */

}
