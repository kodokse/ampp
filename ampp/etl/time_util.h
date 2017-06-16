#pragma once

namespace etl
{

FILETIME operator+(const FILETIME &lhs, const FILETIME &rhs);
FILETIME GetCurrentLocalFileTime();

std::wstring FormatFileTime(const wchar_t *fmt, const FILETIME &ft);

} // namespace etl
