#include "io/rrrAig.h"
#include "io/rrrBinary.h"
#include "network/rrrAndNetwork.h"
#include "extra/rrrTable.h"
#include "extra/rrrCanonicalizer.h"

using namespace rrr;

int main(int argc, char **argv) {
  Table<char> tab(20);

  if(argc > 2) {
    std::cout << argc << std::endl;
    for(int i = 1; i < argc; i++) {
      std::cout << argv[i] << std::endl;
      rrr::AndNetwork ntk;
      ntk.Read(argv[i], rrr::AigFileReader<rrr::AndNetwork>);
      ntk.TrivialCollapse();
      Canonicalizer<rrr::AndNetwork> can;
      can.Run(&ntk);
      std::string str = CreateBinary(&ntk);
      char his = 0;
      int index;
      std::string str2;
      tab.Register(str, his, index, str2);
    }
  } else {
    std::ifstream f(argv[1]);
    std::string line;
    while(std::getline(f, line)) {
      std::cout << line << std::endl;
      rrr::AndNetwork ntk;
      ntk.Read(line, rrr::AigFileReader<rrr::AndNetwork>);
      Canonicalizer<rrr::AndNetwork> can;
      can.Run(&ntk);
      std::string str = CreateBinary(&ntk);
      char his = 0;
      int index;
      std::string str2;
      tab.Register(str, his, index, str2);
    }
  }
  
  std::cout << "unique = " << tab.Size() << std::endl;
  
  return 0;
}
