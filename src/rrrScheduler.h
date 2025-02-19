#pragma once

#include <iostream>
#include <iomanip>
#include <queue>
#include <random>

#ifdef ABC_USE_PTHREADS
#include <thread>
#include <mutex>
#include <condition_variable>
#endif

#include "rrrParameter.h"
#include "rrrTypes.h"

#include "rrrAbc.h"

namespace rrr {

  template <typename Ntk, typename Opt>
  class Scheduler {
  private:
    // job
    struct Job;
    
    // pointer to network
    Ntk *pNtk;

    // parameters
    int nVerbose;
    int iSeed;
    int nFlow;
    int nRestarts;
    bool fMultiThreading;
    seconds nTimeout;
    std::function<double(Ntk *)> CostFunction;

    // copy of parameter (maybe updated during the run). probably unnecessary.
    Parameter Par;
    
    // data
    time_point start;
    std::queue<Job> qPendingJobs;
    Opt *pOpt; // used only in case of single thread execution
#ifdef ABC_USE_PTHREADS
    bool fTerminate;
    std::vector<std::thread> vThreads;
    std::priority_queue<Job> qFinishedJobs;
    std::mutex mutexPendingJobs;
    std::mutex mutexFinishedJobs;
    std::condition_variable condPendingJobs;
    std::condition_variable condFinishedJobs;
#endif

    // time
    seconds GetRemainingTime() const;

    // run job
    void RunJob(Opt &opt, Job const &job) const;

#ifdef ABC_USE_PTHREADS
    // thread
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
  
  template <typename Ntk, typename Opt>
  struct Scheduler<Ntk, Opt>::Job {
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
    
    // smaller id comes first in priority_queue
    bool operator<(const Job& other) const {
      return id > other.id;
    }
  };
  
  /* }}} */

  /* {{{ Time */

  template <typename Ntk, typename Opt>
  seconds Scheduler<Ntk, Opt>::GetRemainingTime() const {
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

  /* {{{ Run job */

  template <typename Ntk, typename Opt>
  void Scheduler<Ntk, Opt>::RunJob(Opt &opt, Job const &job) const {
    opt.UpdateNetwork(job.pNtk);
    // start flow
    switch(nFlow) {
    case 0:
      if(job.id != 0) {
        // TODO: randomization should be triggered by a different flag
        opt.Randomize(job.iSeed);
      }
      opt.Run(GetRemainingTime());
      break;
    case 1: { // transtoch
      std::mt19937 rng(job.iSeed);
      double dCost = CostFunction(job.pNtk);
      double dBestCost = dCost;
      Ntk best(*job.pNtk);
      if(nVerbose) {
        std::cout << "start: cost = " << dCost << std::endl;
      }
      for(int i = 0; i < 10; i++) {
        if(GetRemainingTime() < 0) {
          break;
        }
        if(i != 0) {
          Abc9Execute(job.pNtk, "&if -K 6; &mfs; &st");
          dCost = CostFunction(job.pNtk);
          opt.UpdateNetwork(job.pNtk, true);
          if(nVerbose) {
            std::cout << "hop " << std::setw(3) << i << ": cost = " << dCost << std::endl;
          }
        }
        for(int j = 0; true; j++) {
          if(GetRemainingTime() < 0) {
            break;
          }
          opt.Randomize(rng());
          opt.Run(GetRemainingTime());
          Abc9Execute(job.pNtk, "&dc2");
          double dNewCost = CostFunction(job.pNtk);
          if(nVerbose) {
            std::cout << "\tite " << std::setw(3) << j << ": cost = " << dNewCost << std::endl;
          }
          if(dNewCost < dCost) {
            dCost = dNewCost;
            opt.UpdateNetwork(job.pNtk, true);
          } else {
            break;
          }
        }
        if(dCost < dBestCost) {
          dBestCost = dCost;
          best = *pNtk;
          i = 0;
        }
      }
      *job.pNtk = best;
      if(nVerbose) {
        std::cout << "end: cost = " << dBestCost << std::endl;
      }
      break;
    }
    case 2: { // deep
      std::mt19937 rng(job.iSeed);
      int n = 0;
      double dCost = CostFunction(job.pNtk);
      Ntk best(*job.pNtk);
      if(nVerbose) {
        std::cout << "start: cost = " << dCost << std::endl;
      }
      for(int i = 0; i < 1000000; i++) {
        if(GetRemainingTime() < 0) {
          break;
        }
        // deepsyn
        int fUseTwo = 0;
        std::string pCompress2rs = "balance -l; resub -K 6 -l; rewrite -l; resub -K 6 -N 2 -l; refactor -l; resub -K 8 -l; balance -l; resub -K 8 -N 2 -l; rewrite -l; resub -K 10 -l; rewrite -z -l; resub -K 10 -N 2 -l; balance -l; resub -K 12 -l; refactor -z -l; resub -K 12 -N 2 -l; rewrite -z -l; balance -l";
        unsigned Rand = rng();
        int fDch = Rand & 1;
        //int fCom = (Rand >> 1) & 3;
        int fCom = (Rand >> 1) & 1;
        int fFx  = (Rand >> 2) & 1;
        int KLut = fUseTwo ? 2 + (i % 5) : 3 + (i % 4);
        std::string pComp;
        if ( fCom == 3 )
          pComp = "; &put; " + pCompress2rs + "; " + pCompress2rs + "; " + pCompress2rs + "; &get";
        else if ( fCom == 2 )
          pComp = "; &put; " + pCompress2rs + "; " + pCompress2rs + "; &get";
        else if ( fCom == 1 )
          pComp = "; &put; " + pCompress2rs + "; &get";
        else if ( fCom == 0 )
          pComp = "; &dc2";
        std::string Command = "&dch";
        if(fDch)
          Command += " -f";
        Command += "; &if -a -K " + std::to_string(KLut) + "; &mfs -e -W 20 -L 20";
        if(fFx)
          Command += "; &fx; &st";
        Command += pComp;
        Abc9Execute(job.pNtk, Command);
        if(nVerbose) {
          std::cout << "ite " << std::setw(6) << i << ": cost = " << CostFunction(job.pNtk) << std::endl;
        }
        // rrr
        for(int j = 0; j < n; j++) {
          if(GetRemainingTime() < 0) {
            break;
          }
          opt.UpdateNetwork(job.pNtk, true);
          opt.Randomize(rng());
          opt.Run(GetRemainingTime());
          if(rng() & 1) {
            Abc9Execute(job.pNtk, "&dc2");
          } else {
            Abc9Execute(job.pNtk, "&put; " + pCompress2rs + "; &get");
          }
          if(nVerbose) {
            std::cout << "\trrr " << std::setw(6) << j << ": cost = " << CostFunction(job.pNtk) << std::endl;
          }
        }
        // eval
        double dNewCost = CostFunction(job.pNtk);
        if(dNewCost < dCost) {
          dCost = dNewCost;
          best = *job.pNtk;
        } else {
          n++;
        }
      }
      *job.pNtk = best;
      if(nVerbose) {
        std::cout << "end: cost = " << dCost << std::endl;
      }
      break;
    }
    default:
      assert(0);
    }
  }
  
  /* }}} */

  /* {{{ Thread */

  template <typename Ntk, typename Opt>
  void Scheduler<Ntk, Opt>::Thread(Parameter const *pPar) {
    Opt opt(pPar, CostFunction);
    while(true) {
      Job job(-1, NULL, 0); // place holder
      {
        std::unique_lock<std::mutex> l(mutexPendingJobs);
        while(!fTerminate && qPendingJobs.empty()) {
          condPendingJobs.wait(l);
        }
        if(fTerminate) {
          assert(qPendingJobs.empty());
          return;
        }
        job = std::move(qPendingJobs.front());
        qPendingJobs.pop();
      }
      assert(job.id != -1);
      RunJob(opt, job);
      {
        std::unique_lock<std::mutex> l(mutexFinishedJobs);
        qFinishedJobs.push(job);
        condFinishedJobs.notify_one();
      }
    }
  }

  /* }}} */
  
  /* {{{ Constructors */
  
  template <typename Ntk, typename Opt>
  Scheduler<Ntk, Opt>::Scheduler(Ntk *pNtk, Parameter const *pPar) :
    pNtk(pNtk),
    nVerbose(pPar->nSchedulerVerbose),
    iSeed(pPar->iSeed),
    nFlow(pPar->nSchedulerFlow),
    nRestarts(pPar->nRestarts),
    fMultiThreading(pPar->nThreads > 1),
    nTimeout(pPar->nTimeout),
    Par(*pPar),
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

  template <typename Ntk, typename Opt>
  Scheduler<Ntk, Opt>::~Scheduler() {
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
    if(pOpt) {
      delete pOpt;
    }
  }

  /* }}} */

  /* {{{ Run */
  
  template <typename Ntk, typename Opt>
  void Scheduler<Ntk, Opt>::Run() {
    start = GetCurrentTime();
    // populate queue
    double dCost;
    if(nRestarts > 0) {
      dCost = CostFunction(pNtk);
      for(int i = 0; i < 1 + nRestarts; i++) {
        qPendingJobs.emplace(i, new Ntk(*pNtk), iSeed + i);
      }
    } else {
      qPendingJobs.emplace(0, pNtk, iSeed);
    }
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      for(int i = 0; i < 1 + nRestarts; i++) { // ensure order
        Job job(-1, NULL, 0); // place holder
        {
          std::unique_lock<std::mutex> l(mutexFinishedJobs);
          while(qFinishedJobs.empty() || qFinishedJobs.top().id != i) {
            condFinishedJobs.wait(l);
          }
          job = std::move(qFinishedJobs.top());
          qFinishedJobs.pop();
        }
        if(nRestarts > 0) {
          double dNewCost = CostFunction(job.pNtk);
          if(dNewCost < dCost) {
            dCost = dNewCost;
            *pNtk = *job.pNtk;
          }
        }
      }
    } else {
#endif
      // if single thread
      while(!qPendingJobs.empty()) {
        Job job = std::move(qPendingJobs.front());
        qPendingJobs.pop();
        RunJob(*pOpt, job);
        if(nRestarts > 0) {
          double dNewCost = CostFunction(job.pNtk);
          if(dNewCost < dCost) {
            dCost = dNewCost;
            *pNtk = *job.pNtk;
          }
        }
      }
#ifdef ABC_USE_PTHREADS
    }
#endif
    time_point end = GetCurrentTime();
    double elapsed_seconds = Duration(start, end);
    if(nVerbose) {
      std::cout << "elapsed: " << std::fixed << std::setprecision(3) << elapsed_seconds << "s" << std::endl;
    }
  }

  /* }}} */

}
