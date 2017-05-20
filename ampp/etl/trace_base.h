#pragma once

namespace etl
{

struct TraceFormat;
struct TraceFormatData;

using TraceFormatAddCallback = std::function<void (const GUID &, const wchar_t *, TraceFormat &&)>;
using ProviderCallback = std::function<bool (const TraceFormatAddCallback &)>;
using SourceFileCallback = std::function<void(const GUID &, const ProviderCallback &)>;
//using ProviderEnumerator = std::function<void(const ProviderEnumeratorCallback &)>;
using TraceGuidCallback = std::function<void(const GUID &, const wchar_t *)>;

class TraceProvider
{
public:
  virtual ~TraceProvider() {}
  virtual bool EnumerateTraces(const SourceFileCallback &, const TraceGuidCallback &) const = 0;
};

} // namespace etl

