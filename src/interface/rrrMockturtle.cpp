#include <random>

#include "rrrMockturtle.h"
#include "network/rrrAndNetwork.h"

#ifdef USE_MOCKTURTLE

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <lorina/aiger.hpp>
#include <mockturtle/io/write_aiger.hpp>

#include <mockturtle/algorithms/cleanup.hpp>

#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/algorithms/aig_resub.hpp>
#include <mockturtle/algorithms/resubstitution.hpp>
#include <mockturtle/algorithms/sim_resub.hpp>

#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/window_rewriting.hpp>

#include <mockturtle/algorithms/refactoring.hpp>
#include <mockturtle/algorithms/node_resynthesis/bidecomposition.hpp>
#include <mockturtle/algorithms/node_resynthesis/dsd.hpp>

#include <mockturtle/algorithms/aig_balancing.hpp>
#include <mockturtle/algorithms/balancing.hpp>
#include <mockturtle/algorithms/balancing/sop_balancing.hpp>

#include <mockturtle/algorithms/functional_reduction.hpp>

using namespace mockturtle;

namespace rrr {

  static inline void DebugPrint(std::string log) {
    (void)log;
    //std::cout << log << std::endl;
  }

  template <typename Rng>
  std::string rw(aig_network &aig, Rng &rng) {
    std::string log;
    switch(rng() % 2) {
    case 0: {
      log = "rw";
      xag_npn_resynthesis<aig_network> resyn;
      cut_rewriting_params ps;
      //ps.cut_enumeration_ps;
      ps.cut_enumeration_ps.cut_size = 4;
      ps.allow_zero_gain = rng() & 1;
      ps.use_dont_cares = rng() & 1;
      //ps.candidate_selection_strategy = minimize_weight;
      //ps.min_cand_cut_size = 3u;
      //ps.min_cand_cut_size_override;
      ps.preserve_depth = rng() & 1;
      if(ps.allow_zero_gain) {
        log += " -z";
      }
      if(ps.use_dont_cares) {
        log += " -d";
      }
      if(ps.preserve_depth) {
        log += " -l";
      }
      DebugPrint(log);
      cut_rewriting( aig, resyn, ps );
      break;
    }
    case 1: {
      log = "wrw";
      window_rewriting_params ps;
      //ps.cut_size = 6 + (rng() % 11);
      //ps.num_levels = 5 + (rng() % 6);
      //ps.level_update_strategy = dont_update;
      //ps.max_num_divs = 100;
      log += " -K " + std::to_string(ps.cut_size);
      log += " -L " + std::to_string(ps.num_levels);
      ps.filter_cyclic_substitutions = true;
      DebugPrint(log);
      window_rewriting( aig, ps );
      break;
    }  
    }
    aig = cleanup_dangling( aig );
    return log;
  }

  template <typename Rng>
  std::string rf(aig_network &aig, Rng &rng) {
    std::string log;
    refactoring_params ps;
    //ps.max_pis = 6 + (rng() % 11);
    ps.allow_zero_gain = rng() & 1;
    //ps.use_reconvergence_cut = true;
    //ps.use_dont_cares = true; // crash
    log += " -K " + std::to_string(ps.max_pis);
    if(ps.allow_zero_gain) {
      log += " -z";
    }
    switch(/*rng() % 2*/1) {
    case 0: {
      // slow
      log = "rf-bidec" + log;
      bidecomposition_resynthesis<aig_network> rf_resyn;
      DebugPrint(log);
      refactoring( aig, rf_resyn, ps );
      break;
    }
    case 1: {
      log = "rf-dsd" + log;
      bidecomposition_resynthesis<aig_network> fallback;
      dsd_resynthesis<aig_network, decltype(fallback)> rf_resyn(fallback);
      DebugPrint(log);
      refactoring( aig, rf_resyn, ps );
      break;
    }
    }
    aig = cleanup_dangling( aig );
    return log;
  }
  
  template <typename Rng>
  std::string rs(aig_network &aig, Rng &rng) {
    std::string log;
    resubstitution_params ps;
    ps.max_pis = 6 + (rng() % 7);
    // ps.max_divisors = 150;
    ps.max_inserts = rng() % 3;
    // ps.skip_fanout_limit_for_roots = 1000;
    // ps.skip_fanout_limit_for_divisors = 100;
    // /****** window-based resub engine ******/
    // ps.use_dont_cares = true; // crash
    ps.window_size = 12 + (rng() % 3);
    ps.preserve_depth = rng() & 1;
    // /****** simulation-based resub engine ******/
    // ps.max_clauses = 1000;
    // ps.conflict_limit = 1000;
    // ps.odc_levels = 0;
    // ps.max_trials = 100;
    // ps.max_divisors_k = 50;
    log += " -K " + std::to_string(ps.max_pis);
    log += " -N " + std::to_string(ps.max_inserts);
    switch(rng() % 3) {
    case 0:
      log = "rs" + log;
      DebugPrint(log);
      aig_resubstitution( aig, ps );
      break;
    case 1: {
      log = "wrs" + log;
      log += " -W " + std::to_string(ps.window_size);
      if(ps.preserve_depth) {
        log += " -l";
      }
      depth_view depth_aig{ aig };
      fanout_view fanout_aig{ depth_aig };
      DebugPrint(log);
      aig_resubstitution2( fanout_aig, ps );
      break;
    }
    case 2:
      log = "srs" + log;
      DebugPrint(log);
      sim_resubstitution( aig,  ps );
      break;
    }
    aig = cleanup_dangling( aig );
    return log;
  }

  template <typename Rng>
  void other(aig_network &aig, Rng &rng) {
    switch(rng() % 2) {
    case 0:
      functional_reduction(aig);
      break;
    case 1: {
      aig_balancing_params ps;
      ps.minimize_levels = true;
      ps.fast_mode = false;
      aig_balance( aig, ps );
      break;
    }
      // sop_rebalancing<aig_network> balfn;
      // balancing_params ps;
      // ps.cut_enumeration_ps.cut_size = 4u;
      // aig = balancing(aig, {balfn}, ps);
    }
    aig = cleanup_dangling( aig );
    //if(fVerbose) cout << "other:  " <<  aig.num_gates() << endl;
  }

  template <typename Ntk>
  void MockturtleReader(aig_network const &aig_, Ntk *pNtk) {
    topo_view aig{aig_};
    std::map<aig_network::node, int> m;
    pNtk->Reserve(aig.size());
    auto s0 = aig.get_constant(0);
    auto n0 = aig.get_node(s0);
    assert(aig.constant_value(n0) == 0);
    m[n0] = pNtk->GetConst0();
    aig.foreach_pi([&](auto n) {
      m[n] = pNtk->AddPi();
    });
    aig.foreach_gate([&](auto n) {
      std::vector<int> vFanins;
      std::vector<bool> vCompls;
      aig.foreach_fanin(n, [&](auto s) {
        auto f = aig.get_node(s);
        auto c = aig.is_complemented(s);
        vFanins.push_back(m[f]);
        vCompls.push_back(c);
      });
      m[n] = pNtk->AddAnd(vFanins, vCompls);
    });
    aig.foreach_po([&](auto s) {
      auto f = aig.get_node(s);
      auto c = aig.is_complemented(s);
      pNtk->AddPo(m[f], c);
    });
  }

  template <typename Ntk>
  aig_network *CreateMockturtle(Ntk *pNtk) {
    aig_network *aig = new aig_network;
    std::vector<aig_network::signal> v(pNtk->GetNumNodes());
    v[0] = aig->get_constant(0);
    pNtk->ForEachPi([&](int id) {
      v[id] = aig->create_pi();
    });
    pNtk->ForEachInt([&](int id) {
      assert(pNtk->GetNodeType(id) == rrr::AND);
      auto x = aig->get_constant(1);
      pNtk->ForEachFanin(id, [&](int fi, bool c) {
        if(x == aig->get_constant(1)) {
          x = v[fi];
          if(c) {
            x = aig->create_not(x);
          }
        } else {
          auto y = v[fi];
          if(c) {
            y = aig->create_not(y);
          }
          x = aig->create_and(x, y);
        }
      });
      v[id] = x;
    });
    pNtk->ForEachPoDriver([&](int fi, bool c) {
      auto x = v[fi];
      if(c) {
        x = aig->create_not(x);
      }
      aig->create_po(x);
    });
    return aig;
  }

  template <typename Ntk, typename Rng>
  std::string MockturtlePerformLocal(Ntk *pNtk, Rng &rng) {
    std::string log;
    aig_network *aig = CreateMockturtle(pNtk);
    switch(rng() % 3) {
    case 0:
      log = rw(*aig, rng);
      break;
    case 1:
      log = rf(*aig, rng);
      break;
    case 2:
      log = rs(*aig, rng);
      break;
    }
    pNtk->Read(*aig, MockturtleReader<Ntk>);
    delete aig;
    return log;
  }

  template std::string MockturtlePerformLocal<AndNetwork, std::mt19937>(AndNetwork *pNtk, std::mt19937 &rng);
  
}

#else

namespace rrr {
  
  template <typename Ntk, typename Rng>
  std::string MockturtlePerformLocal(Ntk *pNtk, Rng &rng) {
    (void)pNtk;
    (void)rng;
    std::string log = "mockturtle is disabled";
    return log;
  }

  template std::string MockturtlePerformLocal<AndNetwork, std::mt19937>(AndNetwork *pNtk, std::mt19937 &rng);

}

#endif
