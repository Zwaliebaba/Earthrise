#pragma once

#include "NeuronCoreBase.h"

namespace Neuron::Server
{
  // Console log — prints to stdout so the server operator can see status.
  // Also forwards to DebugTrace for the VS debugger.
  template <class... Types>
  void ServerLog(std::format_string<Types...> _fmt, Types&&... _args)
  {
    std::string msg = std::format(_fmt, std::forward<Types>(_args)...);
    std::printf("%s", msg.c_str());
    std::fflush(stdout);
    DebugTrace("{}", msg);
  }
}
