#pragma once

namespace etl_lib
{

struct TraceFormat;
struct TraceFormatData;

std::wstring FormatTraceFormat(const TraceFormat &fmt, TraceFormatData data);

} // namespace etl_lib

