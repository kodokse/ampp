#pragma once

namespace etl
{

template <class CharT>
std::vector<std::basic_string<CharT>> Split(const CharT *&s, const CharT *end, CharT ch);
template <class CharT>
std::vector<std::basic_string<CharT>> Split(const CharT *&s, const CharT *end, const CharT *match);
template <class CharT>
std::vector<std::basic_string<CharT>> Split(const std::basic_string<CharT> &s, CharT ch);
template <class CharT>
std::vector<std::basic_string<CharT>> Split(const std::basic_string<CharT> &s, const CharT *match);

template <class CharT>
bool IsWhite(CharT c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

template <class CharT, class TraitsT>
std::basic_string<CharT, TraitsT> &TrimR(std::basic_string<CharT, TraitsT> &s)
{
  auto l = s.length();
  while (l > 0 && IsWhite(s[l - 1]))
  {
    --l;
  }
  if (l < s.length())
  {
    s.resize(l);
  }
  return s;
}

void Skip(const wchar_t *&fmtLine, const wchar_t *endFmtLine, wchar_t ch);
void SkipUntil(const wchar_t *&fmtLine, const wchar_t *endFmtLine, wchar_t ch);

template <class CIterT>
std::wstring CopyUntil(CIterT &fmtLine, const CIterT &endFmtLine, wchar_t ch)
{
  std::wstring rv;
  while(fmtLine != endFmtLine && *fmtLine != ch)
  {
    rv += *fmtLine++;
  }
  return rv;
}

template <class CIterT, class PredT>
std::wstring CopyWhile(CIterT &fmtLine, const CIterT &endFmtLine, PredT p)
{
  std::wstring rv;
  while (fmtLine != endFmtLine && p(*fmtLine))
  {
    rv += *fmtLine++;
  }
  return rv;
}

template <class CharT, class IntT>
struct IntTraits
{
private:
  static const int MaxNumRadix = 10;
  static bool IsNumDigit(CharT v, int radix)
  {
    return v >= CharT('0') && v < CharT('0') + __min(radix, MaxNumRadix);
  }
public:
  static bool IsDigit(CharT v, int radix)
  {
    if(IsNumDigit(v, radix))
    {
      return true;
    }
    auto lv = tolower(v);
    return radix > 10 && lv >= CharT('a') && lv < CharT('a') + (radix - MaxNumRadix);
  }
  static int Value(CharT v, int radix)
  {
    return IsNumDigit(v, radix) ? v - CharT('0') : MaxNumRadix + (v - CharT('a'));
  }
};


template <class CIterT, class IntT, std::enable_if_t<std::is_signed_v<IntT>>>
bool ParseInt(CIterT &cur, const CIterT &end, IntT &val, int radix, int maxDigits = -1)
{
  using Traits = IntTraits<typename CIterT::value_type, IntT>;
  bool negate = false;
  auto tmp = cur;
  if(*cur == '-')
  {
    ++cur;
    negate = true;
  }
  if(!Traits::IsDigit(*cur, radix))
  {
    cur = tmp;
    return false;
  }
  int digitCount = 0;
  val = 0;
  while(cur != end && Traits::IsDigit(*cur, radix) && (maxDigits > 0 ? digitCount < maxDigits : true))
  {
    val *= radix;
    val += Traits::Value(*cur, radix);
    ++cur;
    ++digitCount;
  }
  if(negate)
  {
    val = -val;
  }
  return true;
}

template <class CIterT, class IntT>
bool ParseInt(CIterT &cur, const CIterT &end, IntT &val, int radix, int maxDigits = -1)
{
  using Traits = IntTraits<typename CIterT::value_type, IntT>;
  if(!Traits::IsDigit(*cur, radix))
  {
    return false;
  }
  int digitCount = 0;
  val = 0;
  while(cur != end && Traits::IsDigit(*cur, radix) && (maxDigits > 0 ? digitCount < maxDigits : true))
  {
    val *= radix;
    val += Traits::Value(*cur, radix);
    ++cur;
    ++digitCount;
  }
  return true;
}

} // namespace etl

