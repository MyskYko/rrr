#include <iostream>
#include <cxxopts.hpp>

#include "misc/rrrParameter.h"
#include "network/rrrAndNetwork.h"
#include "scheduler/rrrCsoScheduler.h"
#include "optimizer/rrrCsoOptimizer.h"
#include "analyzer/rrrThresholdAnalyzer.h"
#include "simulator/rrrL1Simulator.h"
#include "partitioner/rrrPartitioner.h"
#include "partitioner/rrrLevelBasePartitioner.h"
#include "io/rrrAig.h"

namespace rrr {

  template <typename Ntk>
  void PerformL1(Ntk *pNtk, Parameter const *pPar) {
    Pattern *pPat = NULL;
    if(!pPar->strPattern.empty()) {
      pPat = new Pattern;
      pPat->Read(pPar->strPattern, pNtk->GetNumPis());
      pNtk->RegisterPattern(pPat);
    }
    assert(!pPar->fUseBddCspf && !pPar->fUseBddMspf);
    assert(pPar->nPartitionType == 0);
    CsoScheduler<Ntk, CsoOptimizer<Ntk, ThresholdAnalyzer<Ntk, L1Simulator<Ntk>, int, true>>, Partitioner<Ntk>> sch(pNtk, pPar);
    sch.Run();
    if(pPat) {
      delete pPat;
    }
  }
  
}

int main(int argc, char **argv) {
  cxxopts::Options options("l0", "l0-error-based optimization");

  options.set_width(100);
  
  options.add_options()
    ("input", "Input file names", cxxopts::value<std::string>())
    ("o,output", "Output file name", cxxopts::value<std::string>())
    ("l,log", "Directory name to dump log files", cxxopts::value<std::string>())
    ("R,seed", "Random seed integer", cxxopts::value<int>()->default_value("0"))
    ("T,timeout", "Timeout in seconds (0 = no limit)", cxxopts::value<int>()->default_value("0"))
    ("J,thread", "Number of threads", cxxopts::value<int>()->default_value("1"))
    ("h,help", "Print this usage")
    ;

  options.add_options("Approximation")
    ("F,pattern", "File of simulation patterns", cxxopts::value<std::string>())
    ("H,opattern", "File of output patterns", cxxopts::value<std::string>())
    ("i,apattern", "File of output patterns of other modules", cxxopts::value<std::string>())
    ("j,label", "File of output labels", cxxopts::value<std::string>())
    ("E,error", "Maximum erroneous simulation patterns", cxxopts::value<int>()->default_value("0"))
    ("f,relax", "Increase error threshold on removal", cxxopts::value<bool>()->default_value("false"))
    ;

  options.add_options("Scheduler")
    ("V,verbose", "Verbosity level of scheduler", cxxopts::value<int>()->default_value("0"))
    ("Y,flow", "Scheduling method\n 0: apply method once\n 1: iterate like transtoch\n 2: iterate like deepsyn\n", cxxopts::value<int>()->default_value("0"))
    ("N,job", "Number of jobs to create by restarting or partitioning", cxxopts::value<int>()->default_value("1"))
    ("B,para", "Maximum number of partitions to optimize in parallel", cxxopts::value<int>()->default_value("1"))
    ("d,det", "Ensure deterministic results", cxxopts::value<bool>()->default_value("true"))
    ("e,ent", "Apply \"c2rs; dc2\" after importing changes of partitions", cxxopts::value<bool>()->default_value("false"))
    ;

  options.add_options("Partitioner")
    ("P,vpart", "Verbosity level of partitioner", cxxopts::value<int>()->default_value("0"))
    ("Z,part", "Partitioning method\n 0: distance\n 1: level\n", cxxopts::value<int>()->default_value("0"))
    ("K,pmax", "Maximum partition size (0 = no partitioning)", cxxopts::value<int>()->default_value("0"))
    ("L,pmin", "Minimum partition size", cxxopts::value<int>()->default_value("0"))
    ("I,pimax", "Maximum input size of partition (0 = no limit)", cxxopts::value<int>()->default_value("0"))
    ;    

  options.add_options("Optimizer")
    ("O,vopt", "Verbosity level of optimizer", cxxopts::value<int>()->default_value("0"))
    ("X,opt", "Optimization method\n 0: single-add resub\n 1: multi-add resub\n 2: repeat 0 and 1\n 3: random one meaningful resub\n", cxxopts::value<int>()->default_value("0"))
    ("M,reduce", "Reduction method\n 0: reverse topological order\n 1: reverse topological order (legacy)\n 2: random order\n", cxxopts::value<int>()->default_value("0"))
    ("G,sort", "Fanin sorting method (-1 = random)", cxxopts::value<int>()->default_value("-1"))
    ("D,dist", "Maximum distance between node and new fanin (0 = no limit)", cxxopts::value<int>()->default_value("0"))
    ("g,greedy", "Discard changes that increased the cost", cxxopts::value<bool>()->default_value("true"))
    ("a,isort", "Sort fanins before each run", cxxopts::value<bool>()->default_value("false"))
    ("b,nsort", "Soft fanins before reducing each node", cxxopts::value<bool>()->default_value("true"))
    ;

  options.add_options("Analyzer")
    ("A,vana", "Verbosity level of analyzer", cxxopts::value<int>()->default_value("0"))
    ("U,ana", "Analysis method\n 0: SAT\n 1: BDD (MSPF)\n 2: BDD (CSPF)\n", cxxopts::value<int>()->default_value("0"))
    ;    

  options.add_options("SAT handler")
    ("S,vsat", "Verbosity level of SAT handler", cxxopts::value<int>()->default_value("0"))
    ("C,conf", "Conflict limit (0 = no limit)", cxxopts::value<int>()->default_value("0"))
    ;
  
  options.add_options("Simulator")
    ("Q,vsim", "Verbosity level of simulator", cxxopts::value<int>()->default_value("0"))
    ("W,word", "Number of simualtion words", cxxopts::value<int>()->default_value("10"))
    ;
  
  options.parse_positional({"input"});
  options.positional_help("<input>")
    .show_positional_help();

  auto result = options.parse(argc, argv);
  
  if(result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  std::string input_filename = result["input"].as<std::string>();
  std::string output_filename;
  if(result.count("output")) {
    output_filename = result["output"].as<std::string>();
  }
  
  rrr::Parameter Par;
  Par.fUseApprox = true;

  if(result.count("log")) {
    Par.strTemporary = result["log"].as<std::string>();
  }
  
  Par.iSeed = result["seed"].as<int>();
  Par.nTimeout = result["timeout"].as<int>();
  Par.nThreads = result["thread"].as<int>();

  if(result.count("pattern")) {
    Par.strPattern = result["pattern"].as<std::string>();
  }
  if(result.count("opattern")) {
    Par.strPatternOutput = result["opattern"].as<std::string>();
  }
  if(result.count("apattern")) {
    Par.strPatternOther = result["apattern"].as<std::string>();
  }
  if(result.count("label")) {
    Par.strPatternLabel = result["label"].as<std::string>();
  }
  Par.nRelaxedPatterns = result["error"].as<int>();
  Par.fRelaxOnRemoval = result["relax"].as<bool>();
  
  Par.nSchedulerVerbose = result["verbose"].as<int>();
  Par.nSchedulerFlow = result["flow"].as<int>();
  Par.nJobs = result["job"].as<int>();
  Par.nParallelPartitions = result["para"].as<int>();
  Par.fDeterministic = result["det"].as<bool>();
  Par.fOptOnInsert = result["ent"].as<bool>(); // should this be command string?

  Par.nPartitionerVerbose = result["vpart"].as<int>();
  Par.nPartitionType = result["part"].as<int>();
  Par.nPartitionSize = result["pmax"].as<int>();
  Par.nPartitionSizeMin = result["pmin"].as<int>();
  Par.nPartitionInputMax = result["pimax"].as<int>();
  
  Par.nOptimizerVerbose = result["vopt"].as<int>();
  Par.nOptimizerFlow = result["opt"].as<int>();
  Par.nSortType = result["sort"].as<int>();
  Par.nReductionMethod = result["reduce"].as<int>();
  Par.nDistance = result["dist"].as<int>();
  Par.fGreedy = result["greedy"].as<bool>();
  Par.fSortInitial = result["isort"].as<bool>();
  Par.fSortPerNode = result["nsort"].as<bool>();
  
  Par.nAnalyzerVerbose = result["vana"].as<int>();
  Par.fUseBddCspf = result["ana"].as<int>() == 2;
  Par.fUseBddMspf = result["ana"].as<int>() == 1;

  Par.nSatSolverVerbose = result["vsat"].as<int>();
  Par.nConflictLimit = result["conf"].as<int>();
  
  Par.nSimulatorVerbose = result["vsim"].as<int>();
  Par.nWords = result["word"].as<int>();
  
  /*
  Par.nResynVerbose = nResynVerbose;
  Par.nResynSize = nResynSize;
  Par.nResynInputMax = nResynInputMax;
  Par.nHops = nHops;
  Par.nJumps = nJumps;
  Par.fNoGlobalJump = fNoGlobalJump;

  Par.fExSim = fExSim;
  
  if(pTemporary) {
    Par.strTemporary = std::string(pTemporary);
  }
  if(pCond) {
    Par.strCond = std::string(pCond);
  }
  */

  rrr::AndNetwork ntk;
  ntk.Read(input_filename, rrr::AigFileReader<rrr::AndNetwork>);

  rrr::PerformL1(&ntk, &Par);

  if(!output_filename.empty()) {
    rrr::DumpAig(output_filename, &ntk);
  }

  return 0;
}
