#pragma once
#include "trace_base.h"

namespace etl_lib
{

class PdbProvider
{
public:
  PdbProvider();
  ~PdbProvider();
  ProviderCallback Provide(const fs::path &pdbPath) const;
private:
  fs::path pdbPath_;
  HANDLE process_;
};

} // namespace etl_lib

