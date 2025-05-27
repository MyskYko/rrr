#pragma once

#include <string>
#include <unistd.h> // For mkstemp, close, unlink

#include "rrrAbc.h"

namespace rrr {

  static inline void SystemCall(std::string Command) {
    int r = system(Command.c_str()); // Executes the 'ls -l' command (lists files in detail on Unix-like systems)
    assert(r == 0);
  }

  template <typename Ntk>
  static inline void ExternalAbcExecute(Ntk *pNtk, std::string Command) {
    static constexpr char *pAbc = "~/abc/abc";
    // create file
    char filename[] = "/tmp/rrr_XXXXXX";
    int fd = mkstemp(filename);
    assert(fd != -1);
    close(fd);
    // write aig
    Gia_Man_t *pGia = CreateGia(pNtk, true);
    Gia_AigerWrite(pGia, filename, 0, 0, 0);
    Gia_ManStop(pGia);
    // call external abc
    std::string cmd = pAbc;
    cmd += " -q \"read_aiger ";
    cmd += filename;
    cmd += "; ";
    cmd += Command;
    cmd += "; strash; write_aiger ";
    cmd += filename;
    cmd += "\"";
    SystemCall(cmd);
    // read aig
    pGia = Gia_AigerRead(filename, 0, 0, 0);
    pNtk->Read(pGia, GiaReader<Ntk>);
    Gia_ManStop(pGia);
    unlink(filename);
  }
  
}
