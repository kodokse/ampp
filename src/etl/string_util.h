#pragma once

namespace etl
{

std::vector<std::wstring> Split(const wchar_t *&s, const wchar_t *end, wchar_t ch);
std::vector<std::wstring> Split(const wchar_t *&s, const wchar_t *end, const wchar_t *match);
std::vector<std::wstring> Split(const std::wstring &s, wchar_t ch);
std::vector<std::wstring> Split(const std::wstring &s, const wchar_t *match);

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
bool ParseInt(CIterT &cur, const CIterT &end, IntT &val, int radix)
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
  val = 0;
  while(cur != end && Traits::IsDigit(*cur, radix))
  {
    val *= radix;
    val += Traits::Value(*cur, radix);
    ++cur;
  }
  if(negate)
  {
    val = -val;
  }
  return true;
}

template <class CIterT, class IntT>
bool ParseInt(CIterT &cur, const CIterT &end, IntT &val, int radix)
{
  using Traits = IntTraits<typename CIterT::value_type, IntT>;
  if(!Traits::IsDigit(*cur, radix))
  {
    return false;
  }
  val = 0;
  while(cur != end && Traits::IsDigit(*cur, radix))
  {
    val *= radix;
    val += Traits::Value(*cur, radix);
    ++cur;
  }
  return true;
}

} // namespace etl

