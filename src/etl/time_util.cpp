#include "stdafx.h"
#include <ampp/etl/time_util.h>

namespace etl
{

namespace
{

void AppendZeroPad(std::wstring &v, int n)
{
  if(n < 10)
  {
    v += L'0';
  }
  v += std::to_wstring(n);
}

} // private namespace

FILETIME operator+(const FILETIME &lhs, const FILETIME &rhs)
{
  LARGE_INTEGER li;
  li.HighPart = lhs.dwHighDateTime;
  li.LowPart = lhs.dwLowDateTime;
  LARGE_INTEGER ri;
  ri.HighPart = rhs.dwHighDateTime;
  ri.LowPart = rhs.dwLowDateTime;
  li.QuadPart += ri.QuadPart;
  FILETIME rv;
  rv.dwHighDateTime = li.HighPart;
  rv.dwLowDateTime = li.LowPart;
  return rv;
}

std::wstring FormatFileTime(const wchar_t *fmt, const FILETIME &ft)
{
  std::wstring rv;
  SYSTEMTIME st;
  FileTimeToSystemTime(&ft, &st);
  while(*fmt)
  {
    switch(*fmt)
    {
    case L'Y':
      rv += std::to_wstring(st.wYear);
      break;
    case L'M':
      AppendZeroPad(rv, st.wMonth);
      break;
    case L'D':
      AppendZeroPad(rv, st.wDay);
      break;
    case L'H':
      AppendZeroPad(rv, st.wHour);
      break;
    case L'm':
      AppendZeroPad(rv, st.wMinute);
      break;
    case L'S':
      AppendZeroPad(rv, st.wSecond);
      break;
    case L'l':
      AppendZeroPad(rv, st.wMilliseconds);
      break;
    default:
      rv += *fmt;
    }
    ++fmt;
  }
  return rv;
}

} // namespace etl

