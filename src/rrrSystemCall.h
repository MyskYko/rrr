#pragma once

#include <string>
#include <unistd.h> // For mkstemp, close, unlink

#include "rrrAbc.h"

namespace rrr {

  static inline bool SystemCall(std::string Command) {
    int r = system(Command.c_str());
    return r == 0;
  }

  template <typename Ntk>
  static inline bool ExternalAbcExecute(Ntk *pNtk, std::string Command, bool fCheck = false) {
    static constexpr char *pAbc = "~/abc/abc";
    // create file
    char filenamet[] = "/tmp/rrr_XXXXXX";
    int fd = mkstemp(filenamet);
    assert(fd != -1);
    close(fd);
    // write aig
    char filename[30];
    char filename2[35];
    sprintf(filename, "%s.aig", filenamet);
    Gia_Man_t *pGia = CreateGia(pNtk, true);
    Gia_AigerWrite(pGia, filename, 0, 0, 0);
    Gia_ManStop(pGia);
    // call external abc
    std::string cmd = pAbc;
    if(fCheck) {
      cmd += " -c ";
    } else {
      cmd += " -q ";
    }
    cmd += "\"read_aiger ";
    cmd += filename;
    cmd += "; ";
    cmd += Command;
    cmd += "; strash; ";
    if(fCheck) {
      cmd += "cec -n ";
      cmd += filename;
      cmd += "; ";
      sprintf(filename2, "%s2.aig", filename);
      cmd += "write_aiger ";
      cmd += filename2;
    } else {
      cmd += "write_aiger ";
      cmd += filename;
    }
    cmd += "\"";
    bool r = SystemCall(cmd);
    if(fCheck) {
      pGia = Gia_AigerRead(filename, 0, 0, 0);
      Gia_Man_t * pNew = Gia_AigerRead(filename2, 0, 0, 0);
      if(Cec_ManVerifyTwo(pGia, pNew, 0)) {
        Gia_ManStop(pGia);
        pGia = pNew;
      } else {
        abort();
      }
      unlink(filename2);
    }
    if(r) {
      // read aig
      pGia = Gia_AigerRead(filename, 0, 0, 0);
      pNtk->Read(pGia, GiaReader<Ntk>);
      Gia_ManStop(pGia);
    }
    unlink(filename);
    unlink(filenamet);
    return r;
  }
  
}
