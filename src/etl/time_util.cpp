#include "stdafx.h"
#include <ampp/etl/time_util.h>
#include <ampp/etl/string_util.h>

namespace etl
{

namespace
{

void AppendZeroPad10(std::wstring &v, int n)
{
  if(n < 10)
  {
    v += L'0';
  }
  v += std::to_wstring(n);
}

void AppendZeroPad100(std::wstring &v, int n)
{
  if (n < 100)
  {
    v += L'0';
  }
  if (n < 10)
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

FILETIME GetCurrentLocalFileTime()
{
  FILETIME now;
  SYSTEMTIME st;
  GetLocalTime(&st);
  SystemTimeToFileTime(&st, &now);
  return now;
}

int FileTimeCmp(const FILETIME &lhs, const FILETIME &rhs)
{
  return memcmp(&lhs, &rhs, sizeof(FILETIME));
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
      AppendZeroPad10(rv, st.wMonth);
      break;
    case L'D':
      AppendZeroPad10(rv, st.wDay);
      break;
    case L'H':
      AppendZeroPad10(rv, st.wHour);
      break;
    case L'm':
      AppendZeroPad10(rv, st.wMinute);
      break;
    case L'S':
      AppendZeroPad10(rv, st.wSecond);
      break;
    case L'l':
      AppendZeroPad100(rv, st.wMilliseconds);
      break;
    default:
      rv += *fmt;
    }
    ++fmt;
  }
  return rv;
}

std::optional<FILETIME> ParseFileTime(const wchar_t *fmt, const std::wstring &data)
{
  SYSTEMTIME st = { 0 };
  auto it = data.begin();
  auto end = data.end();
  while (*fmt && it != end)
  {
    switch (*fmt)
    {
    case L'Y':
      if (!ParseInt(it, end, st.wYear, 10, 4))
      {
        return {};
      }
      break;
    case L'M':
      if (!ParseInt(it, end, st.wMonth, 10, 2))
      {
        return {};
      }
      break;
    case L'D':
      if (!ParseInt(it, end, st.wDay, 10, 2))
      {
        return {};
      }
      break;
    case L'H':
      if (!ParseInt(it, end, st.wHour, 10, 2))
      {
        return {};
      }
      break;
    case L'm':
      if (!ParseInt(it, end, st.wMinute, 10, 2))
      {
        return {};
      }
      break;
    case L'S':
      if (!ParseInt(it, end, st.wSecond, 10, 2))
      {
        return {};
      }
      break;
    case L'l':
      if (!ParseInt(it, end, st.wMilliseconds, 10, 3))
      {
        return {};
      }
      break;
    default:
      if (*it != *fmt)
      {
        return {};
      }
      ++it;
    }
    ++fmt;
  }
  FILETIME ft;
  if (!SystemTimeToFileTime(&st, &ft))
  {
    return {};
  }
  return ft;
}

} // namespace etl

