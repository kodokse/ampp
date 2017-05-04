#pragma once

namespace etl_lib
{

struct TraceFormat;
struct TraceFormatData;

using TraceFormatAddCallback = std::function<void (const GUID &, PWSTR, TraceFormat &&)>;
using ProviderCallback = std::function<bool (const TraceFormatAddCallback &)>;

} // namespace etl_lib

