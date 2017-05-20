#pragma once

namespace etl
{

struct TraceFormat;
struct TraceFormatData;

std::wstring FormatTraceFormat(const TraceFormat &fmt, TraceFormatData data);

} // namespace etl

