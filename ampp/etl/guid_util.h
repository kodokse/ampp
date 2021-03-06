#pragma once

namespace etl
{

class GuidLess
{
public:
  bool operator ()(const GUID &lhs, const GUID &rhs) const 
  {
    return memcmp(&lhs, &rhs, sizeof(GUID)) < 0;
  }
};

template <class V>
using GuidKeyedMap = std::map<GUID, V, GuidLess>;

bool GuidFromString(const std::wstring &s, GUID *outGuid);
std::wstring GuidToString(const GUID &g);
std::wstring NewGuidAsString();

} // namespace etl

