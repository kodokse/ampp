#include "stdafx.h"
#include "trace_format_impl.h"

namespace etl_lib
{

TraceFormat::TraceFormat()
  : traceIndex(0)
  , lineNumber(0)
  , traceLevel(0)
  , flags(0)
  , fileInfoFlags(0)
{
}

} // namespace etl_lib

