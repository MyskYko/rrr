#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <set>
#include <string>
#include <functional>
#include <limits>
#include <type_traits>
#include <cassert>

#include "rrrTypes.h"

namespace rrr {

  /* {{{ Invocable */

#if defined(__cpp_lib_is_invocable)
  template <typename Fn, typename... Args>
  using is_invokable = std::is_invocable<Fn, Args...>;
#else
  template <typename Fn, typename... Args>
  struct is_invokable: std::is_constructible<std::function<void(Args...)>, std::reference_wrapper<typename std::remove_reference<Fn>::type>> {};
#endif

#if defined(__cpp_lib_is_invocable)
  template <typename Fn, typename... Args>
  using invoke_return_t = std::invoke_result_t<Fn, Args...>;
#else
  template <typename Fn, typename... Args>
  struct invoke_return {
  private:
    template <typename F, typename... A>
    static auto test(int) -> decltype(std::declval<F>()(std::declval<A>()...));
    template <typename, typename...>
    static void test(...);
  public:
    using type = decltype(test<Fn, Args...>(0));
  };
  template <typename Fn, typename... Args>
  using invoke_return_t = typename invoke_return<Fn, Args...>::type;
#endif

#if defined(__cpp_lib_is_invocable)
  template <typename Fn, typename... Args>
  constexpr bool returns_int_v = std::is_same_v<invoke_return_t<Fn, Args...>, int>;
#else
  template <typename Fn, typename... Args>
  struct returns_int: std::is_same<invoke_return_t<Fn, Args...>, int> {};
  template <typename Fn, typename... Args>
  constexpr bool returns_int_v = returns_int<Fn, Args...>::value;
#endif
  
  /* }}} */

  /* {{{ Int size */
  
  template <template <typename...> typename Container, typename... Ts>
  static inline int int_size(Container<Ts...> const &c) {
    assert(c.size() <= (typename Container<Ts...>::size_type)std::numeric_limits<int>::max());
    return c.size();
  }

  template <template <typename...> typename Container, typename... Ts>
  static inline bool check_int_size(Container<Ts...> const &c) {
    return c.size() <= (typename Container<Ts...>::size_type)std::numeric_limits<int>::max();
  }

  static inline bool check_int_max(int i) {
    return i == std::numeric_limits<int>::max();
  }

  template <typename Iterator>
  static inline int int_distance(Iterator begin, Iterator it) {
    typename std::iterator_traits<Iterator>::difference_type d = std::distance(begin, it);
    assert(d <= (typename std::iterator_traits<Iterator>::difference_type)std::numeric_limits<int>::max());
    assert(d >= (typename std::iterator_traits<Iterator>::difference_type)std::numeric_limits<int>::lowest());
    return d;
  }
  
  /* }}} */

  /* {{{ Print containers */
  
  template <typename T>
  std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
    std::string delim;
    os << "[";
    for(T const &e: v) {
      os << delim << e;
      delim = ", ";
    }
    os << "]";
    return os;
  }
  
  template <typename T>
  std::ostream& operator<<(std::ostream& os, const std::set<T>& s) {
    std::string delim;
    os << "{";
    for(T const &e: s) {
      os << delim << e;
      delim = ", ";
    }
    os << "}";
    return os;
  }

  static inline void PrintComplementedEdges(std::function<void(std::function<void(int, bool)> const &)> const &ForEachEdge) {
    std::string delim;
    std::cout << "[";
    ForEachEdge([&] (int id, bool c) {
      std::cout << delim << (c? "!": "") << id;
      delim = ", ";
    });
    std::cout << "]";
  }

  /* }}} */

  /* {{{ Print next */

  struct SW {
    int width = 0;
    bool left = false;
  };

  struct NS {}; // no space

  template <typename T>
  void PrintNext(std::ostream &os, T t);
  template <typename T, typename... Args>
  void PrintNext(std::ostream &os, T t, Args... args);
  
  static inline void PrintNext(std::ostream &os, int t) {
    os << std::setw(4) << t;
  }

  template <typename... Args>
  static inline void PrintNext(std::ostream &os, int t, Args... args) {
    os << std::setw(4) << t << " ";
    PrintNext(os, args...);
  }

  static inline void PrintNext(std::ostream &os, bool arg) {
    os << arg;
  }
  
  template <typename... Args>
  static inline void PrintNext(std::ostream &os, bool arg, Args... args) {
    if(arg) {
      os << "!";
    } else {
      os << " ";
    }
    PrintNext(os, args...);
  }
  
  static inline void PrintNext(std::ostream &os, double t) {
    //os << std::fixed << std::setprecision(2) << std::setw(8) << t;
    os << std::scientific << std::setprecision(std::numeric_limits<double>::max_digits10) << t;
  }

  template <typename... Args>
  static inline void PrintNext(std::ostream &os, double t, Args... args) {
    //os << std::fixed << std::setprecision(2) << std::setw(8) << t << " ";
    os << std::scientific << std::setprecision(std::numeric_limits<double>::max_digits10) << t << " ";
    PrintNext(os, args...);
  }

  template <typename T>
  static inline void PrintNext(std::ostream &os, SW sw, T arg) {
    if(sw.left) {
      os << std::left;
    }
    os << std::setw(sw.width) << arg;
    if(sw.left) {
      os << std::right;
    }
  }

  template <typename T, typename... Args>
  static inline void PrintNext(std::ostream &os, SW sw, T arg, Args... args) {
    if(sw.left) {
      os << std::left;
    }
    os << std::setw(sw.width) << arg << " ";
    if(sw.left) {
      os << std::right;
    }
    PrintNext(os, args...);
  }

  template <typename T, typename... Args>
  static inline void PrintNext(std::ostream &os, NS ns, T arg, Args... args) {
    (void)ns;
    os << arg;
    PrintNext(os, args...);
  }
  
  template <typename T>
  static inline void PrintNext(std::ostream& os, std::vector<T> const &arg) {
    os << "[ ";
    for(T const &e: arg) {
      PrintNext(os, e);
      os << " ";
    }
    os << "]";
  }

  template <typename T, typename... Args>
  static inline void PrintNext(std::ostream& os, std::vector<T> const &arg, Args... args) {
    os << "[ ";
    for(T const &e: arg) {
      PrintNext(os, e);
      os << " ";
    }
    os << "] ";
    PrintNext(os, args...);
  }

  template <typename T>
  static inline void PrintNext(std::ostream& os, std::set<T> const &arg) {
    os << "{ ";
    for(T const &e: arg) {
      PrintNext(os, e);
      os << " ";
    }
    os << "}";
  }

  template <typename T, typename... Args>
  static inline void PrintNext(std::ostream& os, std::set<T> const &arg, Args... args) {
    os << "{ ";
    for(T const &e: arg) {
      PrintNext(os, e);
      os << " ";
    }
    os << "} ";
    PrintNext(os, args...);
  }
  
  template <typename T>
  void PrintNext(std::ostream &os, T t) {
    os << t;
  }

  template <typename T, typename... Args>
  void PrintNext(std::ostream &os, T t, Args... args) {
    os << t << " ";
    PrintNext(os, args...);
  }  
  
  /* }}} */

  /* {{{ Print others */

  static inline void PrintWarning(std::string message) {
    std::cerr << "[w] " << message << std::endl;
  }

  /* }}} */
  
  /* {{{ Combination */

  inline bool ForEachCombinationStopRec(std::vector<int> &v, int n, int k, std::function<bool(std::vector<int> const &)> const &func) {
    if(k == 0) {
      return func(v);
    }
    for(int i = v.back() + 1; i < n - k + 1; i++) {
      v.push_back(i);
      if(ForEachCombinationStopRec(v, n, k-1, func)) {
        return true;
      }
      v.pop_back();
    }
    return false;
  }
  
  static inline void ForEachCombinationStop(int n, int k, std::function<bool(std::vector<int> const &)> const &func) {
    std::vector<int> v;
    v.reserve(k);
    ForEachCombinationStopRec(v, n, k, func);
  }
  
  /* }}} */

  /* {{{ Random */

  class SimpleRNG {
    static constexpr unsigned NUMBER1 = 3716960521u;
    static constexpr unsigned NUMBER2 = 2174103536u;
    unsigned m_z, m_w;
    
  public:
    SimpleRNG() :
      m_z(NUMBER1),
      m_w(NUMBER2) {
    }
    
    unsigned operator()() {
      m_z = 36969 * (m_z & 65535) + (m_z >> 16);
      m_w = 18000 * (m_w & 65535) + (m_w >> 16);
      return (m_z << 16) + m_w;
    }

    void Reset() {
      m_z = NUMBER1;
      m_w = NUMBER2;
    }

    unsigned long long W() {
      return ((unsigned long long)(*this)() << 32) | ((unsigned long long)(*this)() << 0);
    }
  };
  
  /* }}} */
  
  /* {{{ VarValue functions */
  
  static inline VarValue DecideVarValue(VarValue x) {
    switch(x) {
    case UNDEF:
      assert(0);
    case rrrTRUE:
      return rrrTRUE;
    case rrrFALSE:
      return rrrFALSE;
    case TEMP_TRUE:
      return rrrTRUE;
    case TEMP_FALSE:
      return rrrFALSE;
    default:
      assert(0);
    }
    return UNDEF;
  }

  static inline char GetVarValueChar(VarValue x) {
    switch(x) {
    case UNDEF:
      return 'x';
    case rrrTRUE:
      return '1';
    case rrrFALSE:
      return '0';
    case TEMP_TRUE:
      return 't';
    case TEMP_FALSE:
      return 'f';
    default:
      assert(0);
    }
    return 'X';
  }

  /* }}} */

  /* {{{ Action functions */
  
  static inline char const *GetActionTypeCstr(Action const &action) {
    switch(action.type) {
    case REMOVE_FANIN:
      return "remove fanin";
    case REMOVE_UNUSED:
      return "remove unused";
    case REMOVE_BUFFER:
      return "remove buffer";
    case REMOVE_CONST:
      return "remove const";
    case ADD_FANIN:
      return "add fanin";
    case TRIVIAL_COLLAPSE:
      return "trivial collapse";
    case TRIVIAL_DECOMPOSE:
      return "trivial decompose";
    case SORT_FANINS:
      return "sort fanins";
    case READ:
      return "read";
    case SAVE:
      return "save";
    case LOAD:
      return "load";
    case POP_BACK:
      return "pop back";
    case INSERT:
      return "insert";
    default:
      assert(0);
    }
    return "";
  }

  static inline std::stringstream GetActionDescription(Action const &action) {
    std::stringstream ss;
    ss << GetActionTypeCstr(action);
    std::string delim = " : ";
    if(action.id != -1) {
      ss << delim;
      PrintNext(ss, "node", action.id);
      delim = " , ";
    }
    if(action.fi != -1) {
      ss << delim;
      PrintNext(ss, "fanin", (bool)action.c, action.fi);
      delim = " , ";
    }
    if(action.idx != -1) {
      ss << delim;
      PrintNext(ss, "index", action.idx);
    }
    ss << std::endl;
    if(!action.vFanins.empty()) {
      ss << "fanins : ";
      PrintNext(ss, action.vFanins);
    }
    if(!action.vIndices.empty()) {
      ss << "indices : ";
      PrintNext(ss, action.vIndices);
    }
    if(!action.vFanouts.empty()) {
      ss << "fanouts : ";
      PrintNext(ss, action.vFanouts);
    }
    return ss;
  }

  /* }}} */
  
  /* {{{ Time functions */
  
  static inline time_point GetCurrentTime() {
    return clock_type::now();
  }

  static inline seconds DurationInSeconds(time_point start, time_point end) {
    seconds t = (std::chrono::duration_cast<std::chrono::seconds>(end - start)).count();
    assert(t >= 0);
    return t;
  }

  static inline double Duration(time_point start, time_point end) {
    double t = (std::chrono::duration<double>(end - start)).count();
    assert(t >= 0);
    return t;
  }

  /* }}} */

}
