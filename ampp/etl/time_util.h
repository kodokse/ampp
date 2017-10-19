#pragma once

namespace etl
{

FILETIME operator+(const FILETIME &lhs, const FILETIME &rhs);
FILETIME GetCurrentLocalFileTime();

int FileTimeCmp(const FILETIME &lhs, const FILETIME &rhs);

std::wstring FormatFileTime(const wchar_t *fmt, const FILETIME &ft);
std::optional<FILETIME> ParseFileTime(const wchar_t *fmt, const std::wstring &data);

} // namespace etl
