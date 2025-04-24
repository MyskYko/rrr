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
    bool Register(std::string const &str, int src, std::vector<Action> const &vActions, int &index);
    
    // run jobs
    void RunJob(Opt &opt, Job *pJob);

    // manage jobs
    Job *CreateJob(int src, std::pair<Ntk *, std::vector<Action>> const &target, bool fAdd);
    void CreateJobs(int src, std::vector<std::pair<Ntk *, std::vector<Action>>> const &targets, bool fAdd);
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
    std::pair<Ntk *, std::vector<Action>> target;
    bool fAdd;
    std::string prefix;
    
    // constructor
    Job(int id, int src, std::pair<Ntk *, std::vector<Action>> const &target, bool fAdd) :
      id(id),
      src(src),
      target(target),
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
  bool Scheduler2<Ntk, Opt, Par>::Register(std::string const &str, int src, std::vector<Action> const &vActions, int &index) {
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexTable);
        if(table.count(str)) {
          index = table[str];
          history[index].emplace_back(src, vActions);
          return false;
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
    auto v = opt.Run(pJob->target, pJob->fAdd);
    if(!v.empty()) {
      //Print(0, pJob->prefix, "src", "=", pJob->src, ",", "phase", "=", (pJob->fAdd? "   add": "reduce"), ",", "choice", "=", int_size(v));
      CreateJobs(pJob->src, v, false);
      delete pJob->target.first;
    } else if(!pJob->fAdd && (pJob->target.second.empty() || pJob->target.second.back().type != ADD_FANIN)) {
      Canonicalizer<Ntk> can;
      can.Run(pJob->target.first);
      std::string str = CreateBinary(pJob->target.first);
      int index;
      bool fNew = Register(str, pJob->src, pJob->target.second, index);
      if(fNew) {
        Print(0, pJob->prefix, "src", "=", pJob->src, ",", "phase", "=", (pJob->fAdd? "   add": "reduce"), ",", "choice", "=", int_size(v), ",", "cost", "=", CostFunction(pJob->target.first), ",", "result", "=", index, "(new)");
        std::pair<Ntk *, std::vector<Action>> new_target(pJob->target.first, std::vector<Action>());
        CreateJob(index, new_target, true);
      } else {
        Print(0, pJob->prefix, "src", "=", pJob->src, ",", "phase", "=", (pJob->fAdd? "   add": "reduce"), ",", "choice", "=", int_size(v), ",", "cost", "=", CostFunction(pJob->target.first), ",", "result", "=", index);
        delete pJob->target.first;
      }
    } else {
      //Print(0, pJob->prefix, "src", "=", pJob->src, ",", "phase", "=", (pJob->fAdd? "   add": "reduce"), ",", "choice", "=", int_size(v));
      delete pJob->target.first;
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
  typename Scheduler2<Ntk, Opt, Par>::Job *Scheduler2<Ntk, Opt, Par>::CreateJob(int src, std::pair<Ntk *, std::vector<Action>> const &target, bool fAdd) {
    Job *pJob = new Job(nCreatedJobs++, src, target, fAdd);
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
  void Scheduler2<Ntk, Opt, Par>::CreateJobs(int src, std::vector<std::pair<Ntk *, std::vector<Action>>> const &targets, bool fAdd) {
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexPendingJobs);
        for(auto const &target: targets) {
          Job *pJob = new Job(nCreatedJobs++, src, target, fAdd);
          qPendingJobs.push(pJob);
        }
        condPendingJobs.notify_all();
      }
      return;
    }
#endif
    for(auto const &target: targets) {
      Job *pJob = new Job(nCreatedJobs++, src, target, fAdd);
      qPendingJobs.push(pJob);
    }
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
    bool fPreprocess = false;
    if(fPreprocess) {
      // some preprocessing
      pOpt->Prepare(pOriginal);
      Print(-1, "","preprocessed", "=", CostFunction(pOriginal));
      Canonicalizer<Ntk> can;
      can.Run(pOriginal);
      std::string str = CreateBinary(pOriginal);
      std::vector<Action> vActions;
      int index;
      Register(str, 0, vActions, index);
      
      // create the first job
      Ntk *pNtk = new Ntk;
      pNtk->Read(str, BinaryReader<Ntk>);
      std::pair<Ntk *, std::vector<Action>> input(pNtk, vActions);
      CreateJob(index, input, true);
    } else {
      pOriginal->Sweep(true);
      pOriginal->TrivialCollapse();
      Canonicalizer<Ntk> can;
      can.Run(pOriginal);
      Ntk *pNtk = new Ntk(*pOriginal);
      std::vector<Action> vActions;
      std::pair<Ntk *, std::vector<Action>> input(pNtk, vActions);
      CreateJob(-1, input, false);
    }

    // wait until all jobs are done
    Wait();

    Print(-1, "","unique", "=", nUniques);
    
    // for(std::string s: strs) {
    //   Ntk a;
    //   a.Read(s, BinaryReader<Ntk>);
    //   a.Print();
    // }
  }

  /* }}} */

}
