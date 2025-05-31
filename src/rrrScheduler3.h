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
#include "rrrMockturtle.h"
#include "rrrSystemCall.h"

#include "rrrSimulator2.h"

namespace rrr {

  template <typename Ntk, typename Opt, typename Par>
  class Scheduler3 {
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
    int nHops;
    int nJumps;
    bool fOptOnInsert;
    std::string strOutput;
    seconds nTimeout;
    std::function<double(Ntk *)> CostFunction;

    int nPartitionSizeMin;
    
    // data
    int nCreatedJobs;
    int nFinishedJobs;
    time_point timeStart;
    std::queue<Job *> qPendingJobs;
    
    // used only in case of single thread execution
    Opt *pOpt;
    Par *pParOpt;
    Par *pParResyn;
    Simulator2<Ntk> *pSim;
        
    // summary
    std::vector<std::string> vStatsSummaryKeys;
    std::map<std::string, int> mStatsSummary;
    std::vector<std::string> vTimesSummaryKeys;
    std::map<std::string, double> mTimesSummary;

    // threads
#ifdef ABC_USE_PTHREADS
    bool fTerminate;
    std::vector<std::thread> vThreads;
    std::priority_queue<Job *, std::vector<Job *>, CompareJobPointers> qFinishedJobs;
    std::queue<Job *> qFinishedJobsOOO;
    std::mutex mutexAbc;
    std::mutex mutexPendingJobs;
    std::mutex mutexFinishedJobs;
    std::mutex mutexPrint;
    std::condition_variable condPendingJobs;
    std::condition_variable condFinishedJobs;
#endif

    // print
    template <typename... Args>
    void Print(int nVerboseLevel, std::string prefix, Args... args);
    
    // time
    seconds GetRemainingTime() const;
    double GetElapsedTime() const;

    // scripts
    template <typename Rng>
    std::string AbcLocal(Rng &rng, bool fOdc);
    template <typename Rng>
    std::string AbcLocalMap(Rng &rng);
    template <typename Rng>
    std::string DeepSynOne(unsigned i, Rng &rng, int &fCom);
    template <typename Rng>
    std::string DeepSynExtra(Rng &rng);

    // run jobs
    void RunJob(Opt &opt, Par &parOpt, Par &parResyn, Simulator2<Ntk> &sim, Job *pJob);

    // manage jobs
    Job *CreateJob(Ntk *pNtk_, int column, int stage, int iteration, int last_impr, double costImproved, double costInitial);
    void OnJobEnd(std::function<void(Job *pJob)> const &func);

    // thread
#ifdef ABC_USE_PTHREADS
    void Thread(Parameter const *pPar);
#endif
    
    // summary
    template <typename T>
    void AddToSummary(std::vector<std::string> &keys, std::map<std::string, T> &m, summary<T> const &result) const;

  public:
    // constructors
    Scheduler3(Ntk *pNtk, Parameter const *pPar);
    ~Scheduler3();
    
    // run
    void Run();
  };

  /* {{{ Job */
  
  template <typename Ntk, typename Opt, typename Par>
  struct Scheduler3<Ntk, Opt, Par>::Job {
    // data
    int id;
    Ntk *pNtk;
    int column;
    int stage;
    int iteration;
    int last_impr;
    double costImproved;
    double costInitial;
    std::string prefix;
    std::string log;
    double duration;
    summary<int> stats;
    summary<double> times;
    
    // constructor
    Job(int id, Ntk *pNtk, int column, int stage, int iteration, int last_impr, double costImproved, double costInitial) :
      id(id),
      pNtk(pNtk),
      column(column),
      stage(stage),
      iteration(iteration),
      last_impr(last_impr),
      costImproved(costImproved),
      costInitial(costInitial) {
      std::stringstream ss;
      PrintNext(ss, "job", id, ":");
      prefix = ss.str() + " ";
    }
  };

  template <typename Ntk, typename Opt, typename Par>
  struct Scheduler3<Ntk, Opt, Par>::CompareJobPointers {
    // smaller id comes first in priority_queue
    bool operator()(Job const *lhs, Job const *rhs) const {
      return lhs->id > rhs->id;
    }
  };
  
  /* }}} */

  /* {{{ Print */
  
  template <typename Ntk, typename Opt, typename Par>
  template<typename... Args>
  inline void Scheduler3<Ntk, Opt, Par>::Print(int nVerboseLevel, std::string prefix, Args... args) {
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
  
  /* {{{ Time */

  template <typename Ntk, typename Opt, typename Par>
  inline seconds Scheduler3<Ntk, Opt, Par>::GetRemainingTime() const {
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
  inline double Scheduler3<Ntk, Opt, Par>::GetElapsedTime() const {
    time_point timeCurrent = GetCurrentTime();
    return Duration(timeStart, timeCurrent);
  }

  /* }}} */

  /* {{{ Scripts */
  
  template <typename Ntk, typename Opt, typename Par>
  template <typename Rng>  
  inline std::string Scheduler3<Ntk, Opt, Par>::AbcLocal(Rng &rng, bool fOdc) {
    std::string Command = "&put; ";
    int r = rng() % 5;
    if(fOdc) {
      if(rng() & 1) {
        r = 2;
      } else {
        r = 4;
      }
    }
    switch(r) {
    case 0:
      Command += "rewrite";
      if(rng() & 1) {
        Command += " -z";
      }
      if(rng() & 1) {
        Command += " -l";
      }
      Command += "; ";
      break;
    case 1:
      Command += "refactor";
      if(rng() & 1) {
        Command += " -z";
      }
      if(rng() & 1) {
        Command += " -l";
      }
      Command += "; ";
      break;
    case 2: {
      Command += "resub";
      int nCutSize = 5 + (rng() % 11); // 5-15
      int nAddition = rng() % 4; // 0-3
      Command += " -K " + std::to_string(nCutSize);
      Command += " -N " + std::to_string(nAddition);
      if(fOdc) {
        int nOdcLevel = 1 + (rng() % 9); // 1-9
        Command += " -F " + std::to_string(nOdcLevel);
      }
      if(rng() & 1) {
        Command += " -z";
      }
      if(rng() & 1) {
        Command += " -l";
      }
      Command += "; ";
      break;
    }
    case 3:
      Command += "balance";
      if(rng() & 1) {
        Command += " -l";
      }
      Command += "; ";
      break;
    case 4: {
      Command += "orchestrate";
      int nCutSize = 5 + (rng() % 11); // 5-15
      int nAddition = rng() % 4; // 0-3
      if(fOdc) {
        int nOdcLevel = 1 + (rng() % 9); // 1-9
        Command += " -F " + std::to_string(nOdcLevel);
      }
      Command += " -K " + std::to_string(nCutSize);
      Command += " -N " + std::to_string(nAddition);
      //Command += " -F " + std::to_string(nOdcLevel);
      if(rng() & 1) {
        Command += " -Z";
      }
      if(rng() & 1) {
        Command += " -z";
      }
      if(rng() & 1) {
        Command += " -l";
      }
      Command += "; ";
      break;
    }
    default:
      assert(0);
    }
    Command += "&get";
    return Command;
  }
  
  template <typename Ntk, typename Opt, typename Par>
  template <typename Rng>  
  inline std::string Scheduler3<Ntk, Opt, Par>::AbcLocalMap(Rng &rng) {
    std::string Command = "read_library ";
    switch(rng() % /*6*/5) {
    case 0:
      Command += "and.genlib";
      break;
    case 1:
      Command += "xag.genlib";
      break;
    case 2:
      Command += "mig.genlib";
      break;
    case 3:
      Command += "dot.genlib";
      break;
    case 4:
      Command += "oh.genlib";
      break;
    case 5: // this causes bug
      Command += "mux.genlib";
      break;
    default:
      assert(0);
    }
    Command += "; ";
    if(rng() & 1) {
      Command += "dch";
      if(rng() & 1) {
        Command += " -f";
      }
      Command += "; ";
    }
    switch(rng() % 2) {
    case 0:
      Command += "map";
      if(rng() & 1) {
        Command += " -a";
      }
      break;
    case 1:
      Command += "amap";
      if(rng() & 1) {
        Command += " -m";
      }
      break;
    default:
      assert(0);
    }
    Command += "; ";
    switch(rng() % 2) {
    case 0:
      Command += "mfs";
      break;
    case 1:
      Command += "mfs2";
      break;
    default:
      assert(0);
    }
    if(rng() & 1) {
      Command += " -a";
    }
    if(rng() & 1) {
      Command += " -e";
    }
    Command += "; ";
    Command += "strash";
    return Command;
  }
  
  template <typename Ntk, typename Opt, typename Par>
  template <typename Rng>  
  inline std::string Scheduler3<Ntk, Opt, Par>::DeepSynOne(unsigned i, Rng &rng, int &fCom) {
    int fUseTwo = 0;
    unsigned Rand = rng();
    int fDch = Rand & 1;
    //int fCom = (Rand >> 1) & 3;
    fCom = (Rand >> 1) & 1;
    int fFx  = (Rand >> 2) & 1;
    int KLut = fUseTwo ? 2 + (i % 5) : 3 + (i % 4);
    std::string Command;
    Command += "&dch";
    Command += fDch ? " -f" : "";
    Command += "; ";
    Command += "&if -a -K ";
    Command += std::to_string(KLut);
    Command += "; ";
    Command += "&mfs -e -W 20 -L 20; ";
    Command += fFx ? "&fx; " : "";
    Command += "&st";
    return Command;
  }
  
  template <typename Ntk, typename Opt, typename Par>
  template <typename Rng>  
  inline std::string Scheduler3<Ntk, Opt, Par>::DeepSynExtra(Rng &rng) {
    std::string Command;
    if(rng() & 1) {
      Command += "&dch";
      switch(rng() % 4) {
      case 0:
        break;
      case 1:
        Command += " -f";
        break;
      case 2:
        Command += " -x";
        break;
      case 3:
        Command += " -y";
        break;
      default:
        assert(0);
      }
      Command += "; ";
    }
    Command += "&if -K ";
    int KLut = 3 + (rng() % 6);
    Command += std::to_string(KLut);
    if(rng() & 1) {
      Command += " -a";
    }
    if(rng() & 1) {
      Command += " -e";
    }
    if(rng() & 1) {
      Command += " -m";
    }
    Command += "; ";
    Command += "&mfs";
    //Command += " -W 100000 -F 100000 -D 100000 -L 100000 -C 100000"; // too much
    if(rng() & 1) {
      Command += " -a";
    }
    if(rng() & 1) {
      Command += " -e";
    }
    Command += "; ";
    if(rng() & 1) {
      Command += "&fx; ";
    }
    Command += "&st";
    return Command;
  }
  
  /* }}} */

  /* {{{ Run jobs */

  template <typename Ntk, typename Opt, typename Par>
  void Scheduler3<Ntk, Opt, Par>::RunJob(Opt &opt, Par &parOpt, Par &parResyn, Simulator2<Ntk> &sim, Job *pJob) {
    time_point timeStartLocal = GetCurrentTime();
    int seed = iSeed + (pJob->column + 1) * (pJob->iteration + 1);
    std::mt19937 rng(seed);
    switch(pJob->stage) {
    case 0: {
      // run ABC/mockturtle
      switch(rng() % 2) {
      case 0: {
        std::string cmd = AbcLocal(rng, false);
	pJob->log = cmd;
	//Print(0, pJob->prefix, "column", pJob->column, pJob->log);
        Abc9Execute(pJob->pNtk, cmd);
        break;
      }
      case 1: {
	//std::cout << "colmn " << pJob->column;
        std::string cmd = MockturtlePerformLocal(pJob->pNtk, rng);
	pJob->log = cmd;
        break;
      }
      default:
        assert(0);
      }
      break;
    }
    case 1: {
      switch(rng() % 3) {
      case 0:
        // run transduction
        if(fPartitioning && !parOpt.IsTooSmall(pJob->pNtk)) {
          parOpt.AssignNetwork(pJob->pNtk);
          Ntk *pSubNtk = parOpt.Extract(seed);
          if(pSubNtk) {
	    pJob->log = "transduction part";
	    //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
            opt.AssignNetwork(pSubNtk);
            opt.SetPrintLine([&](std::string str) {
              Print(-1, pJob->prefix, str);
            });
            opt.Run(seed, GetRemainingTime());
            parOpt.Insert(pSubNtk);
            pJob->stats = opt.GetStatsSummary();
            pJob->times = opt.GetTimesSummary();
            opt.ResetSummary();
          }
        } else {
	  pJob->log = "transduction";
	  //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
          opt.AssignNetwork(pJob->pNtk);
          opt.SetPrintLine([&](std::string str) {
            Print(-1, pJob->prefix, str);
          });
          opt.Run(seed, GetRemainingTime());
          pJob->stats = opt.GetStatsSummary();
          pJob->times = opt.GetTimesSummary();
          opt.ResetSummary();
        }
        break;
      case 1: {
        // abc resub with odc
        std::string cmd = AbcLocal(rng, true);
	pJob->log = cmd;
	//Print(0, pJob->prefix, "column", pJob->column, pJob->log);
        Abc9Execute(pJob->pNtk, cmd);
        break;
      }
      case 2: {
        // mapping base optimization
        std::string cmd = AbcLocalMap(rng);
	pJob->log = cmd;
	//Print(0, pJob->prefix, "column", pJob->column, pJob->log);
        ExternalAbcExecute(pJob->pNtk, cmd);
        break;
      }
      default:
        assert(0);
      }
      break;
    }
    case 2: {
      // jump
      switch(rng() % 2) {
      case 0: {
        // partial resynthesis
        parResyn.AssignNetwork(pJob->pNtk);
        Ntk *pSubNtk = parResyn.Extract(seed);
        if(pSubNtk) {
          switch(rng() % 5) {
          case 0: {
	    pJob->log = "ttopt";
	    //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
            std::vector<ABC_UINT64_T> vSdc;
            {
              sim.AssignNetwork(pJob->pNtk, true);
              auto v = sim.ComputeSdc(parResyn.GetInputs(pSubNtk));
              vSdc.resize(v.size());
              for(int i = 0; i < int_size(v); i++) {
                vSdc[i] = v[i];
              }
            }
            Gia_Man_t *pGia = CreateGia(pSubNtk, false);
            Gia_Man_t *pNew = Gia_ManTtoptCare2(pGia, Gia_ManCiNum(pGia), Gia_ManCoNum(pGia), 100, vSdc.data());
            Gia_ManStop(pGia);
            pSubNtk->Read(pNew, GiaReader<Ntk>);
            Gia_ManStop(pNew);
            break;
          }
          case 1: {
            std::string cmd = "&transduction -V 0 -T 3";
	    pJob->log = cmd;
	    //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
            if(rng() & 1) {
              cmd += " -m";
            }
            if(rng() & 1) {
              cmd += " -R " + std::to_string(rng() & 0x7fffffff);
            }
            Abc9Execute(pSubNtk, cmd);
            break;
          }
          case 2: {
            std::string cmd = "collapse; sop; fx";
	    pJob->log = cmd;
	    //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
            ExternalAbcExecute(pSubNtk, cmd);
            break;
          }
          case 3: {
            std::string cmd = "collapse; dsd";
	    pJob->log = cmd;
	    //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
            ExternalAbcExecute(pSubNtk, cmd);
            break;
          }
          case 4: {
            std::string cmd = "collapse; aig; bidec";
	    pJob->log = cmd;
	    //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
            ExternalAbcExecute(pSubNtk, cmd);
            break;
          }
          default:
            assert(0);
          }
          parResyn.Insert(pSubNtk);
          break;
        }
      } // fall back on global resynthesis
      case 1: {
        // global resynthesis
        switch(rng() % 5) {
        case 0: {
          int fCom; // unused
          std::string cmd = DeepSynOne(rng(), rng, fCom);
	  pJob->log = cmd;
	  //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
          Abc9Execute(pJob->pNtk, cmd);
          break;
        }
        case 1: {
          std::string cmd = DeepSynExtra(rng);
	  pJob->log = cmd;
	  //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
          Abc9Execute(pJob->pNtk, cmd);
          break;
        }
        case 2:
	  pJob->log = "&if -y -K 6";
	  //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
          Abc9Execute(pJob->pNtk, "&if -y -K 6; &st");
          break;
        case 3:
	  pJob->log = "&if -g";
	  //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
          Abc9Execute(pJob->pNtk, "&if -g; &st");
          break; 
        case 4:
	  pJob->log = "&if -x";
	  //Print(0, pJob->prefix, "column", pJob->column, pJob->log);
          Abc9Execute(pJob->pNtk, "&if -x; &st");
          break; 
        default:
          assert(0);
        }
        break;
      }
      default:
        assert(0);
      }
      break;
    }
    default:
      assert(0);
    }
    /* GA example
    int nBase = 100, nCoef = 5;
    if(rng() % nBase < nCoef) {
      parResyn.AssignNetwork(pJob->pNtk);
      Ntk *pSubNtk = parResyn.Extract(pJob->iSeed);
      if(pSubNtk) {
        switch(rng() % 1) {
        case 0: {
          std::vector<ABC_UINT64_T> vSdc;
          {
            sim.AssignNetwork(pJob->pNtk, true);
            auto v = sim.ComputeSdc(parResyn.GetInputs(pSubNtk));
            vSdc.resize(v.size());
            for(int i = 0; i < int_size(v); i++) {
              vSdc[i] = v[i];
            }
          }
          Gia_Man_t *pGia = CreateGia(pSubNtk, false);
          Gia_Man_t *pNew = Gia_ManTtoptCare2(pGia, Gia_ManCiNum(pGia), Gia_ManCoNum(pGia), 100, vSdc.data());
          Gia_ManStop(pGia);
          pSubNtk->Read(pNew, GiaReader<Ntk>);
          Gia_ManStop(pNew);
          break;
        }
        case 1:
          if(fMultiThreading) {
            std::unique_lock<std::mutex> l(mutexAbc);
            Abc9Execute(pSubNtk, "&put; collapse; sop; fx; strash; &get");
          } else {
            Abc9Execute(pSubNtk, "&put; collapse; sop; fx; strash; &get");
          }
          break;
        case 2:
          if(fMultiThreading) {
            std::unique_lock<std::mutex> l(mutexAbc);
            Abc9Execute(pSubNtk, "&put; collapse; dsd; strash; &get");
          } else {
            Abc9Execute(pSubNtk, "&put; collapse; dsd; strash; &get");
          }
          break;
        case 3:
          if(fMultiThreading) {
            std::unique_lock<std::mutex> l(mutexAbc);
            Abc9Execute(pSubNtk, "&put; collapse; aig; bidec; strash; &get");
          } else {
            Abc9Execute(pSubNtk, "&put; collapse; aig; bidec; strash; &get");
          }
          break;
        }
        parResyn.Insert(pSubNtk);    
      }
      Abc9Execute(pJob->pNtk, std::string("&put; ") + pCompress2rs + "; dc2; &get");
    } else {
      if(fPartitioning) {
        parOpt.AssignNetwork(pJob->pNtk);
        Ntk *pSubNtk = parOpt.Extract(pJob->iSeed);
        if(pSubNtk) {
          opt.AssignNetwork(pSubNtk);
          opt.SetPrintLine([&](std::string str) {
            Print(-1, pJob->prefix, str);
          });
          opt.Run(pJob->iSeed, GetRemainingTime());
          parOpt.Insert(pSubNtk);
          pJob->stats = opt.GetStatsSummary();
          pJob->times = opt.GetTimesSummary();
          opt.ResetSummary();
        }
      } else {
        opt.AssignNetwork(pJob->pNtk);
        opt.SetPrintLine([&](std::string str) {
          Print(-1, pJob->prefix, str);
        });
        opt.Run(pJob->iSeed, GetRemainingTime());
        pJob->stats = opt.GetStatsSummary();
        pJob->times = opt.GetTimesSummary();
        opt.ResetSummary();
      }
      Abc9Execute(pJob->pNtk, std::string("&put; ") + pCompress2rs + "; dc2; &get");
    }
    */
    pJob->pNtk->Clear(false, true, false);
    time_point timeEndLocal = GetCurrentTime();
    pJob->duration = Duration(timeStartLocal, timeEndLocal);
  }

  /* }}} */

  /* {{{ Manage jobs */

  template <typename Ntk, typename Opt, typename Par>
  typename Scheduler3<Ntk, Opt, Par>::Job *Scheduler3<Ntk, Opt, Par>::CreateJob(Ntk *pNtk_, int column, int stage, int iteration, int last_impr, double costImproved, double costInitial) {
    Job *pJob = new Job(nCreatedJobs++, pNtk_, column, stage, iteration, last_impr, costImproved, costInitial);
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
  void Scheduler3<Ntk, Opt, Par>::OnJobEnd(std::function<void(Job *pJob)> const &func) {
#ifdef ABC_USE_PTHREADS
    if(fMultiThreading) {
      Job *pJob = NULL;
      {
        std::unique_lock<std::mutex> l(mutexFinishedJobs);
        if(fDeterministic) {
          while(qFinishedJobs.empty()) {
            condFinishedJobs.wait(l);
          }
          pJob = qFinishedJobs.top();
          qFinishedJobs.pop();
        } else {
          while(qFinishedJobsOOO.empty()) {
            condFinishedJobs.wait(l);
          }
          pJob = qFinishedJobsOOO.front();
          qFinishedJobsOOO.pop();
        }
      }
      assert(pJob != NULL);
      func(pJob);
      AddToSummary(vStatsSummaryKeys, mStatsSummary, pJob->stats);
      AddToSummary(vTimesSummaryKeys, mTimesSummary, pJob->times);
      delete pJob;
      nFinishedJobs++;
      return;
    }
#endif
    // if single thread
    assert(!qPendingJobs.empty());
    Job *pJob = qPendingJobs.front();
    qPendingJobs.pop();
    RunJob(*pOpt, *pParOpt, *pParResyn, *pSim, pJob);
    func(pJob);
    AddToSummary(vStatsSummaryKeys, mStatsSummary, pJob->stats);
    AddToSummary(vTimesSummaryKeys, mTimesSummary, pJob->times);
    delete pJob;
    nFinishedJobs++;
  }
  
  /* }}} */

  /* {{{ Thread */

#ifdef ABC_USE_PTHREADS
  template <typename Ntk, typename Opt, typename Par>
  void Scheduler3<Ntk, Opt, Par>::Thread(Parameter const *pPar) {
    Abc_Start();
    AbcLmsStart("lib6.aig");
    Opt opt(pPar, CostFunction);
    Par parOpt(pPar);
    Par parResyn(pPar->nResynVerbose, pPar->nResynSize, 0, pPar->nResynInputMax);
    Simulator2<Ntk> sim;
    while(true) {
      Job *pJob = NULL;
      {
        std::unique_lock<std::mutex> l(mutexPendingJobs);
        while(!fTerminate && qPendingJobs.empty()) {
          condPendingJobs.wait(l);
        }
        if(fTerminate) {
          assert(qPendingJobs.empty());
          Abc_Stop();
          return;
        }
        pJob = qPendingJobs.front();
        qPendingJobs.pop();
      }
      assert(pJob != NULL);
      RunJob(opt, parOpt, parResyn, sim, pJob);
      {
        std::unique_lock<std::mutex> l(mutexFinishedJobs);
        if(fDeterministic) {
          qFinishedJobs.push(pJob);
        } else {
          qFinishedJobsOOO.push(pJob);
        }
        condFinishedJobs.notify_one();
      }
    }
  }
#endif

  /* }}} */
  
  /* {{{ Summary */

  template <typename Ntk, typename Opt, typename Par>
  template <typename T>
  void Scheduler3<Ntk, Opt, Par>::AddToSummary(std::vector<std::string> &keys, std::map<std::string, T> &m, summary<T> const &result) const {
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
  Scheduler3<Ntk, Opt, Par>::Scheduler3(Ntk *pNtk, Parameter const *pPar) :
    pNtk(pNtk),
    nVerbose(pPar->nSchedulerVerbose),
    iSeed(pPar->iSeed),
    nFlow(pPar->nSchedulerFlow),
    nJobs(pPar->nJobs),
    fMultiThreading(pPar->nThreads > 1),
    fPartitioning(pPar->nPartitionSize > 0),
    fDeterministic(pPar->fDeterministic),
    nParallelPartitions(pPar->nParallelPartitions),
    nHops(pPar->nHops),
    nJumps(pPar->nJumps),
    fOptOnInsert(pPar->fOptOnInsert),
    strOutput(pPar->strOutput),
    nTimeout(pPar->nTimeout),
    nCreatedJobs(0),
    nFinishedJobs(0),
    pOpt(NULL),
    pParOpt(NULL),
    pParResyn(NULL),
    pSim(NULL) {
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
        vThreads.emplace_back(std::bind(&Scheduler3::Thread, this, pPar));
      }
      return;
    }
#endif
    assert(!fMultiThreading);
    AbcLmsStart("lib6.aig");
    pOpt = new Opt(pPar, CostFunction);
    pParOpt = new Par(pPar);
    pParResyn = new Par(pPar->nResynVerbose, pPar->nResynSize, 0, pPar->nResynInputMax);
    pSim = new Simulator2<Ntk>;
  }

  template <typename Ntk, typename Opt, typename Par>
  Scheduler3<Ntk, Opt, Par>::~Scheduler3() {
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
    delete pParOpt;
    delete pParResyn;
    delete pSim;
  }

  /* }}} */

  /* {{{ Run */

  template <typename Ntk, typename Opt, typename Par>
  void Scheduler3<Ntk, Opt, Par>::Run() {
    timeStart = GetCurrentTime();
    double costStart = CostFunction(pNtk);
    pNtk->Sweep();
    double costBest = CostFunction(pNtk);
    // alias for now
    int nPopulation = nParallelPartitions;
    //int nTimeoutStep = nTimeout;
    // deterministic anyways
    fDeterministic = false;
    // initialize population
    std::vector<Ntk *> vPopulation;
    for(int i = 0; i < nPopulation; i++) {
      vPopulation.push_back(new Ntk(*pNtk));
    }
    // search
    bool fFirst = true;
    std::mt19937 rng(iSeed);
    std::vector<int> vIterations(nPopulation);
    while(GetRemainingTime() >= 0 && nCreatedJobs < nJobs) {
      // create jobs (jump if not first)
      for(int i = 0; i < nPopulation; i++) {
        Job *pJob;
        if(fFirst) {
          pJob = CreateJob(vPopulation[i], i, 0, 0, 0, CostFunction(vPopulation[i]), CostFunction(vPopulation[i]));
        } else {
          pJob = CreateJob(vPopulation[i], i, 2, vIterations[i], vIterations[i], 1000000 /* big number */, CostFunction(vPopulation[i]));
        }
        Print(1, pJob->prefix, "created ", ":", "i/o", "=", pJob->pNtk->GetNumPis(), "/", pJob->pNtk->GetNumPos(), ",", "node", "=", pJob->pNtk->GetNumInts(), ",", "level", "=", pJob->pNtk->GetNumLevels(), ",", "cost", "=", pJob->costInitial);
      }
      // loop until next jump
      while(nFinishedJobs < nCreatedJobs) {
        OnJobEnd([&](Job *pJob) {
          // check
          double cost = CostFunction(pJob->pNtk);          
          Print(1, pJob->prefix, "finished", ":", "i/o", "=", pJob->pNtk->GetNumPis(), "/", pJob->pNtk->GetNumPos(), ",", "node", "=", pJob->pNtk->GetNumInts(), ",", "level", "=", pJob->pNtk->GetNumLevels(), ",", "cost", "=", cost);
          //Print(0, "", "job", pJob->id, "(", nFinishedJobs + 1, "/", nJobs, ")", ":", "i/o", "=", pJob->pNtk->GetNumPis(), "/", pJob->pNtk->GetNumPos(), ",", "node", "=", pJob->pNtk->GetNumInts(), ",", "level", "=", pJob->pNtk->GetNumLevels(), ",", "cost", "=", cost, "(", 100 * (cost - pJob->costInitial) / pJob->costInitial, "%", ")", ",", "duration", "=", pJob->duration, "s", ",", "elapsed", "=", GetElapsedTime(), "s");
	  Print(0, "", "job", pJob->id, "(", nFinishedJobs + 1, "/", nJobs, ")", ":", pJob->pNtk->GetNumPis(), "/", pJob->pNtk->GetNumPos(), ",", pJob->pNtk->GetNumInts(), ",", pJob->pNtk->GetNumLevels(), ",", cost, "(", 100 * (cost - pJob->costInitial) / pJob->costInitial, "%", ")", ",", pJob->column, ",", pJob->stage, ",", pJob->iteration, ",", pJob->last_impr, ",", pJob->duration, "s", ",", GetElapsedTime(), "s", ",", pJob->log);
          if(cost < costBest) {
            pNtk->Read(*(pJob->pNtk));
            costBest = cost;
	    if(!strOutput.empty()) {
	      DumpAig(strOutput, pNtk, true);
	    }
          }
          // next job
          int column = pJob->column;
          int iteration = pJob->iteration + 1;
          int last_impr;
          double costImproved;
          if(cost < pJob->costImproved) {
            last_impr = pJob->iteration;
            costImproved = cost;
          } else {
            last_impr = pJob->last_impr;
            costImproved = pJob->costImproved;
          }
          if(GetRemainingTime() < 0 || nCreatedJobs >= nJobs || (nJumps && iteration - last_impr == nJumps)) {
            vIterations[pJob->column] = pJob->iteration + 1;
            pJob = NULL; // end or wait for jump
          } else if(nHops && (iteration - last_impr) % nHops == 0) {
            pJob = CreateJob(pJob->pNtk, column, 1, iteration, last_impr, costImproved, cost);
          } else {
            pJob = CreateJob(pJob->pNtk, column, 0, iteration, last_impr, costImproved, cost);
          }
          if(pJob) {
            Print(1, pJob->prefix, "created ", ":", "i/o", "=", pJob->pNtk->GetNumPis(), "/", pJob->pNtk->GetNumPos(), ",", "node", "=", pJob->pNtk->GetNumInts(), ",", "level", "=", pJob->pNtk->GetNumLevels(), ",", "cost", "=", pJob->costInitial);
          }
        });
      }
      // print
      {
        std::vector<double> costs;
        for(Ntk *pNtk_: vPopulation) {
          costs.push_back(CostFunction(pNtk_));
        }
        std::sort(costs.begin(), costs.end());
        Print(-1, "", "optimized", "(", nFinishedJobs, " / ", nJobs, ")", ":", costs, ",", GetElapsedTime(), "s");
      }
      // sync
      double costMin = CostFunction(vPopulation[0]);
      for(int i = 1; i < nPopulation; i++) {
        double cost = CostFunction(vPopulation[i]);
        if(cost < costMin) {
          costMin = cost;
        }
      }
      std::vector<Ntk *> vBests;
      for(int i = 0; i < nPopulation; i++) {
        if(costMin == CostFunction(vPopulation[i])) {
          vBests.push_back(vPopulation[i]);
        }
      }
      for(int i = 0; i < nPopulation; i++) {
        if(costMin != CostFunction(vPopulation[i])) {
          delete vPopulation[i];
          vPopulation[i] = new Ntk(*(vBests[rng() % vBests.size()]));
        }
      }
      fFirst = false;
    }
    // delete
    for(Ntk *pNtk_: vPopulation) {
      delete pNtk_;
    }
    vPopulation.clear();


    // width is nParallelPartitions, gives thread id for each
    // nJobs decides max iterations
    // job has multiple stages
    // for some number of iterations, do stage 0 (abc, mockturtle)
    // do random resub (H) or hop
    // do resyn (M) or jump (if, lms, other balancing maybe)
    
    
    /* GA example
    // alias for now
    int nPopulation = nParallelPartitions;
    // extra parameters
    int nTBase = 99;
    int nTCoef = 100;
    // initialize population
    std::vector<Ntk *> vPopulation;
    for(int i = 0; i < nPopulation; i++) {
      Ntk *pCopy = new Ntk(*pNtk);
      vPopulation.push_back(pCopy);
    }
    // search
    std::mt19937 rng(iSeed);
    while(nCreatedJobs < nJobs) {
      // keep best
      //vPopulation.push_back(new Ntk(*pNtk));
      // print
      {
        std::vector<double> costs;
        for(Ntk *pNtk_: vPopulation) {
          costs.push_back(CostFunction(pNtk_));
        }
        std::sort(costs.begin(), costs.end());
        std::cout << nFinishedJobs << " / " << nJobs << ": ";
        std::cout << costs << std::endl;
      }
      // create jobs from current generation
      for(int i = 0; i < nPopulation; i++) {
        Ntk *pSelected = NULL;
        // do tournament selection
        pSelected = vPopulation[rng() % nPopulation];
        double cost = CostFunction(pSelected);
        int nTournament = 0;
        for(int j = 0; j < nTournament; j++) {
          Ntk *pNext = vPopulation[rng() % nPopulation];
          double costNext = CostFunction(pNext);
          if(costNext < cost) {
            pSelected = pNext;
            cost = costNext;
          }
        }
        // dispatch
        Ntk *pCopy = new Ntk(*pSelected);
        Job *pJob = CreateJob(pCopy, iSeed + nCreatedJobs, cost);
        Print(1, pJob->prefix, "created ", ":", "i/o", "=", pJob->pNtk->GetNumPis(), "/", pJob->pNtk->GetNumPos(), ",", "node", "=", pJob->pNtk->GetNumInts(), ",", "level", "=", pJob->pNtk->GetNumLevels(), ",", "cost", "=", pJob->costInitial);
      }
      // clear current generation
      for(Ntk *pNtk_: vPopulation) {
        delete pNtk_;
      }
      vPopulation.clear();
      // get new generation
      while(nFinishedJobs < nCreatedJobs) {
        OnJobEnd([&](Job *pJob) {
          vPopulation.push_back(pJob->pNtk);
          double cost = CostFunction(pJob->pNtk);
          Print(1, pJob->prefix, "finished", ":", "i/o", "=", pJob->pNtk->GetNumPis(), "/", pJob->pNtk->GetNumPos(), ",", "node", "=", pJob->pNtk->GetNumInts(), ",", "level", "=", pJob->pNtk->GetNumLevels(), ",", "cost", "=", cost);
          Print(0, "", "job", pJob->id, "(", nFinishedJobs + 1, "/", nJobs, ")", ":", "i/o", "=", pJob->pNtk->GetNumPis(), "/", pJob->pNtk->GetNumPos(), ",", "node", "=", pJob->pNtk->GetNumInts(), ",", "level", "=", pJob->pNtk->GetNumLevels(), ",", "cost", "=", cost, "(", 100 * (cost - pJob->costInitial) / pJob->costInitial, "%", ")", ",", "duration", "=", pJob->duration, "s", ",", "elapsed", "=", GetElapsedTime(), "s");
          if(cost < costBest) {
            pNtk->Read(*(pJob->pNtk));
            costBest = cost;
            DumpAig("best.aig", pNtk, true);
          }
        });
      }
    }
    // delete
    for(Ntk *pNtk_: vPopulation) {
      delete pNtk_;
    }
    vPopulation.clear();
    */
    // summary
    double cost = CostFunction(pNtk);
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
