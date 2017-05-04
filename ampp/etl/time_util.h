#pragma once

namespace etl_lib
{

FILETIME operator+(const FILETIME &lhs, const FILETIME &rhs);

std::wstring FormatFileTime(const wchar_t *fmt, const FILETIME &ft);

} // namespace etl_lib
