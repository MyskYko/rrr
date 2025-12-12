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
#include "interface/rrrAbc.h"
#include "io/rrrBinary.h"
#include "io/rrrAig.h"
#include "extra/rrrTable.h"
#include "extra/rrrCanonicalizer.h"

namespace rrr {

  template <typename Ntk, typename Opt, typename Par>
  class SsrScheduler2 {
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
    std::string strTemporary;
    std::function<double(Ntk *)> CostFunction;
    static constexpr bool fTwoArgSym = false;
    std::vector<std::string> CommandList;
    
    // data
    Table<std::vector<int>> tab;
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
    bool Register(Ntk *pNtk, int src, std::vector<int> const &vCommands, int &index);
    
    // run jobs
    void RunJob(Opt &opt, Job *pJob);

    // manage jobs
    Job *CreateJob(int src);
    void Wait();

    // thread
#ifdef ABC_USE_PTHREADS
    void Thread(Parameter const *pPar);
#endif

  public:
    // constructors
    SsrScheduler2(Ntk *pNtk, Parameter const *pPar);
    ~SsrScheduler2();
    
    // run
    std::vector<Ntk *> Run();
  };

  /* {{{ Job */
  
  template <typename Ntk, typename Opt, typename Par>
  struct SsrScheduler2<Ntk, Opt, Par>::Job {
    // data
    int id;
    int src;
    std::string prefix;
    
    // constructor
    Job(int id, int src) :
      id(id),
      src(src) {
      std::stringstream ss;
      PrintNext(ss, "job", id, ":");
      prefix = ss.str() + " ";
    }
  };
  
  /* }}} */

  /* {{{ Print */
  
  template <typename Ntk, typename Opt, typename Par>
  template<typename... Args>
  inline void SsrScheduler2<Ntk, Opt, Par>::Print(int nVerboseLevel, std::string prefix, Args... args) {
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
  bool SsrScheduler2<Ntk, Opt, Par>::Register(Ntk *pNtk, int src, std::vector<int> const &vCommands, int &index) {
    std::string str, str_sym;
    {
      Ntk ntk;
      ntk.Read(*pNtk);
      Canonicalizer<Ntk> can;
      can.Run(&ntk, true);
      str = CreateBinary(&ntk, true);
      if(fTwoArgSym) {
        std::vector<int> vOrder;
        for(int i = 0; i < ntk.GetNumPis() / 2; i++) {
          vOrder.push_back(i + ntk.GetNumPis() / 2);
        }
        for(int i = 0; i < ntk.GetNumPis() / 2; i++) {
          vOrder.push_back(i);
        }        
        ntk.ChangePiOrder(vOrder);
        can.Run(&ntk, true);
        str_sym = CreateBinary(&ntk, true);
      }
    }
    std::vector<int> his;
    {
      his.push_back(src);
      for(int command: vCommands) {
        his.push_back(command);
      }
    }
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      {
        std::unique_lock<std::mutex> l(mutexTable);
        return tab.Register(str, his, index, str_sym);
      }
    }
#endif
    return tab.Register(str, his, index, str_sym);
  }
  
  /* }}} */
    
  /* {{{ Run jobs */

  template <typename Ntk, typename Opt, typename Par>
  void SsrScheduler2<Ntk, Opt, Par>::RunJob(Opt &opt, Job *pJob) {
    Ntk ntk;
    (void)opt;
    // opt.AssignNetwork(&ntk, pJob->fAdd);
    // opt.SetPrintLine([&](std::string str) {
    //   Print(-1, pJob->prefix, str);
    // });
    int nChoices = 0, nNews = 0;
    for(int i = 0; i < int_size(CommandList); i++) {
      ntk.Read(tab.Get(pJob->src), BinaryReader<Ntk>);
      std::string cmd = "&put; ";
      cmd += CommandList[i];
      cmd += "; &get";
      Abc9Execute(&ntk, cmd);
      std::vector<int> vCommands {i};
      int index;
      bool fNew = Register(&ntk, pJob->src, vCommands, index);
      if(fNew) {
        //if(index % 1000 == 0)
        Print(0, pJob->prefix, "src", "=", pJob->src, ",", "choice", "=", nChoices, "new", "=", nNews, ",", "cost", "=", CostFunction(&ntk), ",", "result", "=", index, "(new)");
        if(!strTemporary.empty()) {
          std::string filename = strTemporary + "_" + std::to_string(index) + ".aig";
          rrr::DumpAig(filename, &ntk);
        }
        if(tab.Size() < 200000) {
          CreateJob(index);
        }
        nNews++;
      } else {
        //Print(0, pJob->prefix, "src", "=", pJob->src, ",", "choice", "=", nChoices, ",", "cost", "=", CostFunction(&ntk), ",", "result", "=", index);
      }
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
  typename SsrScheduler2<Ntk, Opt, Par>::Job *SsrScheduler2<Ntk, Opt, Par>::CreateJob(int src) {
    Job *pJob = new Job(nCreatedJobs++, src);
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
  void SsrScheduler2<Ntk, Opt, Par>::Wait() {
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
  void SsrScheduler2<Ntk, Opt, Par>::Thread(Parameter const *pPar) {
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
  SsrScheduler2<Ntk, Opt, Par>::SsrScheduler2(Ntk *pNtk, Parameter const *pPar) :
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
    tab(27),
    nCreatedJobs(0),
    nFinishedJobs(0) {
    CostFunction = [](Ntk *pNtk) {
      int nTwoInputSize = 0;
      pNtk->ForEachInt([&](int id) {
        nTwoInputSize += pNtk->GetNumFanins(id) - 1;
      });
      return nTwoInputSize;
    };
    CommandList = {"balance",
                   "balance -l",
                   //"rewrite",
                   //"rewrite -l",
                   "rewrite -z",
                   "rewrite -zl",
                   //"refactor",
                   //"refactor -l",
                   "refactor -z",
                   "refactor -zl"};
    for(int K = 4; K <= 16; K++) {
      for(int N = 0; N <= 3; N++) {
        std::string command = "resub";
        command += " -N ";
        command += std::to_string(N);
        command += " -K ";
        command += std::to_string(K);
        //CommandList.push_back(command);
        //CommandList.push_back(command + " -l");
        CommandList.push_back(command + " -z");
        CommandList.push_back(command + " -zl");
      }
    }
    pOpt = new Opt(pPar);
#ifdef ABC_USE_PTHREADS
    fTerminate = false;
    if(fMultiThreading) {
      vThreads.reserve(pPar->nThreads);
      for(int i = 0; i < pPar->nThreads; i++) {
        vThreads.emplace_back(std::bind(&SsrScheduler2::Thread, this, pPar));
      }
    }
#endif
  }

  template <typename Ntk, typename Opt, typename Par>
  SsrScheduler2<Ntk, Opt, Par>::~SsrScheduler2() {
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
  std::vector<Ntk *> SsrScheduler2<Ntk, Opt, Par>::Run() {
    std::vector<int> vCommands;
    int index;
    Register(pOriginal, -1, vCommands, index);
    CreateJob(index);

    // wait until all jobs are done
    Wait();

    Print(-1, "","unique", "=", tab.Size());
    Print(-1, "","jobs", "=", nFinishedJobs);

    // another table with canonicalization with trivial collapse
    Table<std::vector<int>> tab2(27);
    for(int i = 0; i < tab.Size(); i++) {
      std::string str = tab.Get(i);
      Ntk ntk;
      ntk.Read(str, BinaryReader<Ntk>);
      //ntk.Sweep(true);
      ntk.TrivialCollapse();
      //bool fRedundant = pOpt->IsRedundant(&ntk);
      Canonicalizer<Ntk> can;
      can.Run(&ntk, true);
      str = CreateBinary(&ntk);
      int index;
      std::vector<int> his{i};
      tab2.Register(str, his, index, "");
    }

    Print(-1, "","unique2", "=", tab2.Size());

    std::vector<Ntk *> vNtks;
    for(int i = 0; i < tab2.Size(); i++) {
      std::string str = tab2.Get(i);
      Ntk *pNtk = new Ntk;
      pNtk->Read(str, BinaryReader<Ntk>);
      vNtks.push_back(pNtk);
    }
    return vNtks;
  }

  /* }}} */

}
