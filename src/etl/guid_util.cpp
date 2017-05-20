#include "stdafx.h"
#include <ampp/etl/guid_util.h>

using namespace std::string_literals;

namespace etl
{

bool GuidFromString(const std::wstring &s, GUID *outGuid)
{
  auto guidStr = L"{"s;
  guidStr += s;
  guidStr += L'}';
  return CLSIDFromString(guidStr.c_str(), outGuid) == S_OK;
}

std::wstring GuidToString(const GUID &g)
{
  std::wstring rv;
  rv.resize(50);
  auto l = StringFromGUID2(g, &rv[0], rv.size());
  if(l > 0)
  {
    rv.resize(l - 1);
  }
  else
  {
    rv.clear();
  }
  return rv;
}

} // namespace etl

