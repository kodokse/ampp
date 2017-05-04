#pragma once

namespace etl_lib
{

using ConfigMap = std::map<std::wstring, std::wstring>;

struct TypeValue
{
  std::wstring type;
  std::wstring argName;
};

enum FILE_INFO_FLAGS {
  FIF_32BIT_TRACE = 0x00,
  FIF_64BIT_TRACE = 0x01,
};

struct TraceFormat
{
  std::wstring moduleName;
  std::wstring function;
  std::wstring formatString;
  std::map<DWORD, TypeValue> typeMap;
  ConfigMap traceCfg;
  DWORD traceIndex;
  DWORD lineNumber;
  DWORD traceLevel;
  DWORD flags;
  DWORD fileInfoFlags;
  //
  TraceFormat();
};

} // namespace etl_lib

