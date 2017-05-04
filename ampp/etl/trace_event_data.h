#pragma once

namespace etl_lib
{

struct TraceEventData
{
  std::wstring moduleName;
  std::wstring function;
  std::wstring message;
  fs::path sourceFile;
  FILETIME timeStamp;
  DWORD threadId;
  DWORD processId;
  DWORD lineNumber;
};

enum class TraceEventDataItem : int
{
  TraceIndex,
  ModuleName,
  Function,
  SourceFile,
  LineNumber,
  TimeStamp,
  ThreadId,
  ProcessId,
  Message,
  MAX_ITEM
};

inline TraceEventDataItem &operator++(TraceEventDataItem &item)
{
  item = static_cast<TraceEventDataItem>(static_cast<int>(item) + 1);
  return item;
}

} // namespace etl_lib

