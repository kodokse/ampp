#include "stdafx.h"

#include <ampp/etl/string_util.h>
#include "trace_util.h"
#include "trace_format_impl.h"
#include "trace_format_data_impl.h"
#include <comdef.h>

namespace etl
{

const wchar_t *GetNtStatusText(DWORD status);

namespace
{

template<class T>
std::wstring ToString(T v, const std::wstring &spec)
{
  std::wstring fmt = L"%";
  fmt += spec;
  wchar_t tmp[40];
  swprintf_s(tmp, fmt.c_str(), v);
  return tmp;
}

std::wstring GetErrorMessageString(DWORD s)
{
  LPWSTR msgBuf = nullptr;
  DWORD langId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US); //MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
  auto len = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
                            GetModuleHandleW(L"ntdll.dll"),
                            s,
                            langId,
                            reinterpret_cast<LPWSTR>(&msgBuf),
                            0,
                            NULL);
  std::wstring rv;
  if (msgBuf)
  {
    rv.assign(msgBuf, len);
    LocalFree(msgBuf);
    TrimR(rv);
  }
  else
  {
    rv = ToString(s, L"X");
  }
  return rv;
}

bool InsertTraceData(const std::wstring &type, const TraceFormat &fmt, TraceFormatData &data, const std::wstring &typeExtra, std::wstring &out)
{
  static const std::map<std::wstring, std::function<void (const TraceFormat &, TraceFormatData &, const std::wstring &, std::wstring &)>> mappers {
    {L"FUNC", [](const TraceFormat &fmt, TraceFormatData &, const std::wstring &, std::wstring &out)
              {
                out += fmt.function;
              }
    },
    { L"LINE", [](const TraceFormat &fmt, TraceFormatData &, const std::wstring &, std::wstring &out)
              {
                out += std::to_wstring(fmt.lineNumber);
              }
    },
    {L"ItemNTSTATUS", [](const TraceFormat &fmt, TraceFormatData &data, const std::wstring &, std::wstring &out)
              {
                if (data.ValidFor(sizeof(NTSTATUS)))
                {
                  auto errTxt = GetNtStatusText(data.As<NTSTATUS>());
                  out += errTxt ? errTxt : GetErrorMessageString(data.As<NTSTATUS>());
                  data.Advance(sizeof(NTSTATUS));
                  }
              }
    },
    { L"ItemHRESULT", [](const TraceFormat &fmt, TraceFormatData &data, const std::wstring &, std::wstring &out)
              {
                out += data.ValidFor(sizeof(HRESULT)) ? GetErrorMessageString(data.As<HRESULT>()) : L"";
                data.Advance(sizeof(HRESULT));
              }
    },
    {L"ItemLong", [](const TraceFormat &fmt, TraceFormatData &data, const std::wstring &typeExtra, std::wstring &out)
              {
                if(!data.ValidFor(sizeof(LONG)))
                {
                  return;
                }
                out += ToString(data.As<LONG>(), typeExtra);
                data.Advance(sizeof(LONG));
              }
    },
    {L"ItemLongLongX", [](const TraceFormat &fmt, TraceFormatData &data, const std::wstring &typeExtra, std::wstring &out)
              {
                if(!data.ValidFor(sizeof(LONGLONG)))
                {
                  return;
                }
                out += ToString(data.As<LONGLONG>(), typeExtra);
                data.Advance(sizeof(LONGLONG));
              }
    },
    {L"ItemULongLong", [](const TraceFormat &fmt, TraceFormatData &data, const std::wstring &typeExtra, std::wstring &out)
              {
                if(!data.ValidFor(sizeof(LONGLONG)))
                {
                  return;
                }
                out += ToString(data.As<LONGLONG>(), typeExtra);
                data.Advance(sizeof(LONGLONG));
              }
    },
    {L"ItemString", [](const TraceFormat &fmt, TraceFormatData &data, const std::wstring &typeExtra, std::wstring &out)
              {
                if(data.ValidFor(sizeof(char)))
                {
                  auto str = static_cast<char *>(data.data);
                  auto len = strlen(str);
                  auto actualLen = __min(len, data.length);
                  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
                  out += converter.from_bytes(str);
                  data.Advance(actualLen + 1);
                }
              }
    },
    {L"ItemWString", [](const TraceFormat &fmt, TraceFormatData &data, const std::wstring &typeExtra, std::wstring &out)
              {
                if(data.ValidFor(sizeof(wchar_t)))
                {
                  auto str = static_cast<wchar_t *>(data.data);
                  auto len = wcslen(str);
                  auto actualLen = __min(len, data.length);
                  out.append(str, actualLen);
                  data.Advance(actualLen + 1);
                }
              }
    },
    {L"ItemPWString", [](const TraceFormat &fmt, TraceFormatData &data, const std::wstring &typeExtra, std::wstring &out)
              {
                if(data.ValidFor(sizeof(wchar_t)))
                {
                  auto str = static_cast<wchar_t *>(data.data);
                  auto len = *str++;
                  auto actualLen = __min(len, data.length) / sizeof(wchar_t);
                  out.append(str, actualLen);
                  data.Advance((1 + actualLen) * sizeof(wchar_t));
                }
              }
    },
    {L"ItemPtr", [](const TraceFormat &fmt, TraceFormatData &data, const std::wstring &typeExtra, std::wstring &out)
              {
                const auto ptrSize = (fmt.fileInfoFlags & FIF_64BIT_TRACE) != 0 ? 8 : 4;
                if(data.ValidFor(ptrSize))
                {
                  std::wstringstream fmtTxt;
                  fmtTxt << L"0" << (ptrSize * 2) << L"X";
                  if ((fmt.fileInfoFlags & FIF_64BIT_TRACE) != 0)
                  {
                    out += ToString(data.As<DWORD64>(), fmtTxt.str());
                  }
                  else
                  {
                    out += ToString(data.As<DWORD>(), fmtTxt.str());
                  }
                  data.Advance(ptrSize);
                }
              }
    },
  };
  auto it = mappers.find(type);
  if(it == mappers.end())
  {
    //std::wcerr << L"NO MAPPER FOR: " << type << std::endl;
    MessageBoxW(nullptr, type.c_str(), L"NO MAPPER FOR", MB_OK);
    return false;
  }
  it->second(fmt, data, typeExtra, out);
  return true;
}

} // private namespace


std::wstring FormatTraceFormat(const TraceFormat &fmt, TraceFormatData data)
{
  std::wstring rv;
  auto cur = fmt.formatString.cbegin();
  auto end = fmt.formatString.cend();
  DWORD tmpIdx;
  while(cur != end)
  {
    if(*cur == L'%')
    {
      ++cur;
      if(*cur == L'!')
      {
        ++cur;
        auto type = CopyUntil(cur, end, L'!');
        if(cur != end)
        {
          ++cur;
        }
        InsertTraceData(type, fmt, data, L"", rv);
      }
      else
      {
        ParseInt(cur, end, tmpIdx, 10);
        std::wstring typeExtra;
        if(*cur == L'!')
        {
          ++cur;
          typeExtra = CopyUntil(cur, end, L'!');
          if(cur != end)
          {
            ++cur;
          }
        }
        auto typeInfo = fmt.typeMap.find(tmpIdx);
        if(typeInfo != fmt.typeMap.end())
        {
          InsertTraceData(typeInfo->second.type, fmt, data, typeExtra, rv);
        }
      }
    }
    else
    {
      rv += *cur++;
    }
  }
  return rv;
}

} // namespace etl

