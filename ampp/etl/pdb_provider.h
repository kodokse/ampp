#pragma once
#include <ampp/etl/trace_base.h>

namespace etl
{

class PdbFileManager;

class PdbProvider : public TraceProvider
{
  class Impl;
public:
  PdbProvider(const PdbFileManager &fm, const fs::path &pdbFile);
  ~PdbProvider();
  //ProviderEnumerator Provide(const fs::path &pdbPath) const;
  bool EnumerateTraces(const SourceFileCallback &, const TraceGuidCallback &) const override;
private:
  std::unique_ptr<Impl> impl_;
};

} // namespace etl

