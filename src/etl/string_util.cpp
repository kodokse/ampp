#include "stdafx.h"
#include "string_util.h"

namespace etl_lib
{

std::vector<std::wstring> Split(const wchar_t *&s, const wchar_t *end, wchar_t ch)
{
  std::vector<std::wstring> rv;
  std::wstring tmp;
  while(s != end && *s)
  {
    if(*s == ch)
    {
      if(!tmp.empty())
      {
        rv.push_back(std::move(tmp));
      }
    }
    else
    {
      tmp += *s;
    }
    ++s;
  }
  if(!tmp.empty())
  {
    rv.push_back(std::move(tmp));
  }
  return rv;
}

std::vector<std::wstring> Split(const wchar_t *&s, const wchar_t *end, const wchar_t *match)
{
  std::vector<std::wstring> rv;
  std::wstring tmp;
  std::wstring tmp2;
  auto cur = match;
  while(s != end && *s)
  {
    if(*s == *cur)
    {
      ++cur;
      tmp2 += *s;
      if(*cur == 0)
      {
        tmp2.clear();
        if(!tmp.empty())
        {
          rv.push_back(std::move(tmp));
        }
        cur = match;
      }
    }
    else
    {
      if(!tmp2.empty())
      {
        tmp += tmp2;
        tmp2.clear();
      }
      tmp += *s;
      cur = match;
    }
    ++s;
  }
  if(!tmp.empty())
  {
    rv.push_back(std::move(tmp));
  }
  return rv;
}

std::vector<std::wstring> Split(const std::wstring &s, wchar_t ch)
{
  std::vector<std::wstring> rv;
  std::wstring tmp;
  for(auto cv : s)
  {
    if(cv == ch)
    {
      if(!tmp.empty())
      {
        rv.push_back(std::move(tmp));
      }
    }
    else
    {
      tmp += cv;
    }
  }
  if(!tmp.empty())
  {
    rv.push_back(std::move(tmp));
  }
  return rv;
}

std::vector<std::wstring> Split(const std::wstring &s, const wchar_t *match)
{
  std::vector<std::wstring> rv;
  std::wstring tmp;
  std::wstring tmp2;
  auto cur = match;
  for(auto cv : s)
  {
    if(cv == *cur)
    {
      ++cur;
      tmp2 += cv;
      if(*cur == 0)
      {
        cur = match;
        tmp2.clear();
        if(!tmp.empty())
        {
          rv.push_back(std::move(tmp));
        }
      }
    }
    else
    {
      cur = match;
      if(!tmp2.empty())
      {
        tmp += tmp2;
        tmp2.clear();
      }
      tmp += cv;
    }
  }
  if(!tmp.empty())
  {
    rv.push_back(std::move(tmp));
  }
  return rv;
}

void Skip(const wchar_t *&fmtLine, const wchar_t *endFmtLine, wchar_t ch)
{
  while(fmtLine != endFmtLine && *fmtLine == ch)
  {
    ++fmtLine;
  }
}

void SkipUntil(const wchar_t *&fmtLine, const wchar_t *endFmtLine, wchar_t ch)
{
  while(fmtLine != endFmtLine && *fmtLine != ch)
  {
    ++fmtLine;
  }
}

} // namespace etl_lib

