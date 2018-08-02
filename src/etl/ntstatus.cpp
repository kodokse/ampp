#include "stdafx.h"
#include "ntstatusx.h"

namespace etl
{

#define X(n) case n: return L#n;

const wchar_t *GetNtStatusText(DWORD status)
{
  switch (status)
  {
    AMPP_NTSTATUS_LIST
  default:
    return nullptr;
  }
}

#undef X

} // namespace etl

