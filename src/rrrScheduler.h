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

namespace rrr {

  template <typename Ntk, typename Opt, typename Par>
  class Scheduler {
  private:
    // aliases
    static constexpr char *pCompress2rs = "balance -l; resub -K 6 -l; rewrite -l; resub -K 6 -N 2 -l; refactor -l; resub -K 8 -l; balance -l; resub -K 8 -N 2 -l; rewrite -l; resub -K 10 -l; rewrite -z -l; resub -K 10 -N 2 -l; balance -l; resub -K 12 -l; refactor -z -l; resub -K 12 -N 2 -l; rewrite -z -l; balance -l";
    
    // job
    struct Job;
    struct CompareJobPointers;
    
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
    seconds nTimeout;
    std::function<double(Ntk *)> CostFunction;
    
    // data
    int nCreatedJobs;
    int nFinishedJobs;
    time_point start;
    Par par;
    std::queue<Job *> qPendingJobs;
    Opt *pOpt; // used only in case of single thread execution
#ifdef ABC_USE_PTHREADS
    bool fTerminate;
    std::vector<std::thread> vThreads;
    std::priority_queue<Job *, std::vector<Job *>, CompareJobPointers> qFinishedJobs;
    std::mutex mutexAbc;
    std::mutex mutexPendingJobs;
    std::mutex mutexFinishedJobs;
    std::mutex mutexPrint;
    std::condition_variable condPendingJobs;
    std::condition_variable condFinishedJobs;
#endif

    // print
    template<typename... Args>
    void Print(int nVerboseLevel, std::string prefix, Args... args);
    
    // time
    seconds GetRemainingTime() const;

    // abc
    void CallAbc(Ntk *pNtk_, std::string Command);

    // run jobs
    void RunJob(Opt &opt, Job const *pJob);

    // manage jobs
    void CreateJob(Ntk *pNtk_, int iSeed_);
    void OnJobEnd(std::function<void(Job *pJob)> const &func);

    // thread
#ifdef ABC_USE_PTHREADS
    void Thread(Parameter const *pPar);
#endif
    
  public:
    // constructors
    Scheduler(Ntk *pNtk, Parameter const *pPar);
    ~Scheduler();
    
    // run
    void Run();
  };

  /* {{{ Job */
  
  template <typename Ntk, typename Opt, typename Par>
  struct Scheduler<Ntk, Opt, Par>::Job {
    // data
    int id;
    Ntk *pNtk;
    int iSeed;
    
    // constructor
    Job(int id, Ntk *pNtk, int iSeed) :
      id(id),
      pNtk(pNtk),
      iSeed(iSeed) {
    }
  };

  template <typename Ntk, typename Opt, typename Par>
  struct Scheduler<Ntk, Opt, Par>::CompareJobPointers {
    // smaller id comes first in priority_queue
    bool operator()(Job const *lhs, Job const *rhs) const {
      return lhs->id > rhs->id;
    }
  };
  
  /* }}} */

  /* {{{ Print */
  
  template <typename Ntk, typename Opt, typename Par>
  template<typename... Args>
  inline void Scheduler<Ntk, Opt, Par>::Print(int nVerboseLevel, std::string prefix, Args... args) {
    if(nVerbose <= nVerboseLevel) {
      return;
    }
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexPrint);
        std::cout << prefix;
        for(int i = 0; i < nVerboseLevel; i++) {
          std::cout << "\t";
        }
        PrintNext(std::cout, args...);
        std::cout << std::endl;
      }
      return;
    }
#endif
    std::cout << prefix;
    for(int i = 0; i < nVerboseLevel; i++) {
      std::cout << "\t";
    }
    PrintNext(std::cout, args...);
    std::cout << std::endl;
  }

  /* }}} */
  
  /* {{{ Time */

  template <typename Ntk, typename Opt, typename Par>
  inline seconds Scheduler<Ntk, Opt, Par>::GetRemainingTime() const {
    if(nTimeout == 0) {
      return 0;
    }
    time_point current = GetCurrentTime();
    seconds nRemainingTime = nTimeout - DurationInSeconds(start, current);
    if(nRemainingTime == 0) { // avoid glitch
      return -1;
    }
    return nRemainingTime;
  }

  /* }}} */

  /* {{{ Abc */

  template <typename Ntk, typename Opt, typename Par>
  inline void Scheduler<Ntk, Opt, Par>::CallAbc(Ntk *pNtk_, std::string Command) {
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexAbc);
        Abc9Execute(pNtk_, Command);
      }
      return;
    }
#endif
    Abc9Execute(pNtk_, Command);
  }

  /* }}} */
  
  /* {{{ Run jobs */

  template <typename Ntk, typename Opt, typename Par>
  void Scheduler<Ntk, Opt, Par>::RunJob(Opt &opt, Job const *pJob) {
    std::string prefix;
    {
      std::stringstream ss;
      PrintNext(ss, "job", pJob->id, ":");
      prefix = ss.str() + " ";
    }
    opt.SetVerbosePrefix(prefix);
    opt.UpdateNetwork(pJob->pNtk);
    // start flow
    switch(nFlow) {
    case 0:
      opt.Run(pJob->iSeed, GetRemainingTime());
      break;
    case 1: { // transtoch
      std::mt19937 rng(pJob->iSeed);
      double dCost = CostFunction(pJob->pNtk);
      double dBestCost = dCost;
      Ntk best(*(pJob->pNtk));
      for(int i = 0; i < 10; i++) {
        if(GetRemainingTime() < 0) {
          break;
        }
        if(i != 0) {
          CallAbc(pJob->pNtk, "&if -K 6; &mfs; &st");
          dCost = CostFunction(pJob->pNtk);
          opt.UpdateNetwork(pJob->pNtk, true);
          Print(1, prefix, "hop", i, ":", "cost", "=", dCost);
        }
        for(int j = 0; true; j++) {
          if(GetRemainingTime() < 0) {
            break;
          }
          opt.Run(rng(), GetRemainingTime());
          CallAbc(pJob->pNtk, "&dc2");
          double dNewCost = CostFunction(pJob->pNtk);
          Print(1, prefix, "ite", j, ":", "cost", "=", dNewCost);
          if(dNewCost < dCost) {
            dCost = dNewCost;
            opt.UpdateNetwork(pJob->pNtk, true);
          } else {
            break;
          }
        }
        if(dCost < dBestCost) {
          dBestCost = dCost;
          best = *(pJob->pNtk);
          i = 0;
        }
      }
      *(pJob->pNtk) = best;
      break;
    }
    case 2: { // deep
      std::mt19937 rng(pJob->iSeed);
      int n = 0;
      double dCost = CostFunction(pJob->pNtk);
      Ntk best(*(pJob->pNtk));
      for(int i = 0; i < 1000000; i++) {
        if(GetRemainingTime() < 0) {
          break;
        }
        // deepsyn
        int fUseTwo = 0;
        unsigned Rand = rng();
        int fDch = Rand & 1;
        //int fCom = (Rand >> 1) & 3;
        int fCom = (Rand >> 1) & 1;
        int fFx  = (Rand >> 2) & 1;
        int KLut = fUseTwo ? 2 + (i % 5) : 3 + (i % 4);
        std::string pComp;
        if ( fCom == 3 )
          pComp = std::string("; &put; ") + pCompress2rs + "; " + pCompress2rs + "; " + pCompress2rs + "; &get";
        else if ( fCom == 2 )
          pComp = std::string("; &put; ") + pCompress2rs + "; " + pCompress2rs + "; &get";
        else if ( fCom == 1 )
          pComp = std::string("; &put; ") + pCompress2rs + "; &get";
        else if ( fCom == 0 )
          pComp = "; &dc2";
        std::string Command = "&dch";
        if(fDch)
          Command += " -f";
        Command += "; &if -a -K " + std::to_string(KLut) + "; &mfs -e -W 20 -L 20";
        if(fFx)
          Command += "; &fx; &st";
        Command += pComp;
        CallAbc(pJob->pNtk, Command);
        Print(1, prefix, "ite", i, ":", "cost", "=", CostFunction(pJob->pNtk));
        // rrr
        for(int j = 0; j < n; j++) {
          if(GetRemainingTime() < 0) {
            break;
          }
          opt.UpdateNetwork(pJob->pNtk, true);
          opt.Run(rng(), GetRemainingTime());
          if(rng() & 1) {
            CallAbc(pJob->pNtk, "&dc2");
          } else {
            CallAbc(pJob->pNtk, std::string("&put; ") + pCompress2rs + "; &get");
          }
          Print(1, prefix, "rrr", j, ":", "cost", "=", CostFunction(pJob->pNtk));
        }
        // eval
        double dNewCost = CostFunction(pJob->pNtk);
        if(dNewCost < dCost) {
          dCost = dNewCost;
          best = *(pJob->pNtk);
        } else {
          n++;
        }
      }
      *(pJob->pNtk) = best;
      break;
    }
    case 3:
      opt.Run(pJob->iSeed, GetRemainingTime());
      CallAbc(pJob->pNtk, std::string("&put; ") + pCompress2rs + "; dc2; &get");
      break;
    default:
      assert(0);
    }
  }

  /* }}} */

  /* {{{ Manage jobs */

  template <typename Ntk, typename Opt, typename Par>
  void Scheduler<Ntk, Opt, Par>::CreateJob(Ntk *pNtk_, int iSeed_) {
    Job *pJob = new Job(nCreatedJobs++, pNtk_, iSeed_);
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexPendingJobs);
        qPendingJobs.push(pJob);
        condPendingJobs.notify_one();
      }
      return;
    }
#endif
    qPendingJobs.push(pJob);
  }
  
  template <typename Ntk, typename Opt, typename Par>
  void Scheduler<Ntk, Opt, Par>::OnJobEnd(std::function<void(Job *pJob)> const &func) {
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      Job *pJob = NULL;
      {
        std::unique_lock<std::mutex> l(mutexFinishedJobs);
        while(qFinishedJobs.empty() || (fDeterministic && qFinishedJobs.top()->id != nFinishedJobs)) {
          condFinishedJobs.wait(l);
        }
        pJob = qFinishedJobs.top();
        qFinishedJobs.pop();
      }
      assert(pJob != NULL);
      func(pJob);
      delete pJob;
      nFinishedJobs++;
      return;
    }
#endif
    // if single thread
    assert(!qPendingJobs.empty());
    Job *pJob = qPendingJobs.front();
    qPendingJobs.pop();
    RunJob(*pOpt, pJob);
    func(pJob);
    delete pJob;
    nFinishedJobs++;
  }
  
  /* }}} */

  /* {{{ Thread */

#ifdef ABC_USE_PTHREADS
  template <typename Ntk, typename Opt, typename Par>
  void Scheduler<Ntk, Opt, Par>::Thread(Parameter const *pPar) {
    Opt opt(pPar, CostFunction);
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
      {
        std::unique_lock<std::mutex> l(mutexFinishedJobs);
        qFinishedJobs.push(pJob);
        condFinishedJobs.notify_one();
      }
    }
  }
#endif

  /* }}} */
  
  /* {{{ Constructors */

  template <typename Ntk, typename Opt, typename Par>
  Scheduler<Ntk, Opt, Par>::Scheduler(Ntk *pNtk, Parameter const *pPar) :
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
    nTimeout(pPar->nTimeout),
    nCreatedJobs(0),
    nFinishedJobs(0),
    par(pPar),
    pOpt(NULL) {
    // prepare cost function
    CostFunction = [](Ntk *pNtk) {
      int nTwoInputSize = 0;
      pNtk->ForEachInt([&](int id) {
        nTwoInputSize += pNtk->GetNumFanins(id) - 1;
      });
      return nTwoInputSize;
    };
#ifdef ABC_USE_PTHREADS
    fTerminate = false;
    if(fMultiThreading) {
      vThreads.reserve(pPar->nThreads);
      for(int i = 0; i < pPar->nThreads; i++) {
        vThreads.emplace_back(std::bind(&Scheduler::Thread, this, pPar));
      }
      return;
    }
#endif
    assert(!fMultiThreading);
    pOpt = new Opt(pPar, CostFunction);
  }

  template <typename Ntk, typename Opt, typename Par>
  Scheduler<Ntk, Opt, Par>::~Scheduler() {
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
      return;
    }
#endif
    delete pOpt;
  }

  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Opt, typename Par>
  void Scheduler<Ntk, Opt, Par>::Run() {
    start = GetCurrentTime();
    double startCost = CostFunction(pNtk);
    if(fPartitioning) {
      fDeterministic = false; // it is deterministic anyways as we wait until all jobs finish each round
      pNtk->Sweep();
      par.UpdateNetwork(pNtk);
      while(nCreatedJobs < nJobs) {
        assert(nParallelPartitions > 0);
        if(nCreatedJobs < nFinishedJobs + nParallelPartitions) {
          Ntk *pSubNtk = par.Extract(iSeed + nCreatedJobs);
          if(pSubNtk) {
            CreateJob(pSubNtk, iSeed + nCreatedJobs);
            std::cout << "job " << nCreatedJobs - 1 << " created (size = " << pSubNtk->GetNumInts() << ")" << std::endl;
            continue;
          }
        }
        if(nCreatedJobs == nFinishedJobs) {
          std::cout << "failed to partition" << std::endl;
          break;
        }
        while(nFinishedJobs < nCreatedJobs) {
          OnJobEnd([&](Job *pJob) {
            std::cout << "job " << pJob->id << " finished (size = " << pJob->pNtk->GetNumInts() << ")" << std::endl;
            par.Insert(pJob->pNtk);
          });
        }
        if(fOptOnInsert) {
          CallAbc(pNtk, std::string("&put; ") + pCompress2rs + "; dc2; &get");
          par.UpdateNetwork(pNtk);
        }
      }
      while(nFinishedJobs < nCreatedJobs) {
        OnJobEnd([&](Job *pJob) {
          std::cout << "job " << pJob->id << " finished (size = " << pJob->pNtk->GetNumInts() << ")" << std::endl;
          par.Insert(pJob->pNtk);
        });
      }
      if(fOptOnInsert) {
        CallAbc(pNtk, std::string("&put; ") + pCompress2rs + "; dc2; &get");
        par.UpdateNetwork(pNtk);
      }
    } else if(nJobs > 1) {
      double dCost = CostFunction(pNtk);
      for(int i = 0; i < nJobs; i++) {
        Ntk *pCopy = new Ntk(*pNtk);
        CreateJob(pCopy, iSeed + i);
      }
      for(int i = 0; i < nJobs; i++) {
        OnJobEnd([&](Job *pJob) {
          double dNewCost = CostFunction(pJob->pNtk);
          if(nVerbose) {
            std::cout << "run " << pJob->id << ": cost = " << dNewCost << std::endl;
          }
          if(dNewCost < dCost) {
            dCost = dNewCost;
            *pNtk = *(pJob->pNtk);
          }
          delete pJob->pNtk;
        });
      }
    } else {
      CreateJob(pNtk, iSeed);
      OnJobEnd([&](Job *pJob) { (void)pJob; });
    }
    time_point end = GetCurrentTime();
    double elapsed_seconds = Duration(start, end);
    if(nVerbose) {
      std::cout << "elapsed: " << std::fixed << std::setprecision(3) << elapsed_seconds << "s" << std::endl;
    }
    double endCost = CostFunction(pNtk);
    double percentage = 100 * (startCost - endCost) / startCost;
    Print(0, "", "cost", ":", startCost, "->", endCost, NS{}, "(", percentage, "%", "reduction)");
  }

  /* }}} */

}
