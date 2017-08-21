#include "stdafx.h"
#include "string_util.h"

namespace etl
{

template <class CharT>
std::vector<std::basic_string<CharT>> Split(const CharT *&s, const CharT *end, CharT ch)
{
  using StringT = std::basic_string<CharT>;
  std::vector<StringT> rv;
  StringT tmp;
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

template <class CharT>
std::vector<std::basic_string<CharT>> Split(const CharT *&s, const CharT *end, const CharT *match)
{
  using StringT = std::basic_string<CharT>;
  std::vector<StringT> rv;
  StringT tmp;
  StringT tmp2;
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

template <class CharT>
std::vector<std::basic_string<CharT>> Split(const std::basic_string<CharT> &s, CharT ch)
{
  using StringT = std::basic_string<CharT>;
  std::vector<StringT> rv;
  StringT tmp;
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

template <class CharT>
std::vector<std::basic_string<CharT>> Split(const std::basic_string<CharT> &s, const CharT *match)
{
  using StringT = std::basic_string<CharT>;
  std::vector<StringT> rv;
  StringT tmp;
  StringT tmp2;
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

//////////////////////////////////////

template std::vector<std::basic_string<wchar_t>> Split(const wchar_t *&s, const wchar_t *end, wchar_t ch);
template std::vector<std::basic_string<wchar_t>> Split(const wchar_t *&s, const wchar_t *end, const wchar_t *match);
template std::vector<std::basic_string<wchar_t>> Split(const std::basic_string<wchar_t> &s, wchar_t ch);
template std::vector<std::basic_string<wchar_t>> Split(const std::basic_string<wchar_t> &s, const wchar_t *match);

template std::vector<std::basic_string<char>> Split(const char *&s, const char *end, char ch);
template std::vector<std::basic_string<char>> Split(const char *&s, const char *end, const char *match);
template std::vector<std::basic_string<char>> Split(const std::basic_string<char> &s, char ch);
template std::vector<std::basic_string<char>> Split(const std::basic_string<char> &s, const char *match);


} // namespace etl

