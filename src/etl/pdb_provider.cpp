#include "stdafx.h"
#include <DbgHelp.h>
#include <ampp/etl/pdb_provider.h>
#include <ampp/etl/guid_util.h>
#include "string_util.h"
#include "trace_format_impl.h"

#pragma comment(lib, "dbghelp.lib")

namespace etl_lib
{

namespace
{

struct TraceContext
{
  HANDLE process;
  bool is64Bit;
  TraceFormatAddCallback addFmt;
};

class AutoSymModule
{
public:
  AutoSymModule(HANDLE process, DWORD64 moduleBase)
    : process_(process)
    , moduleBase_(moduleBase)
  {
  }
  ~AutoSymModule()
  {
    if(process_ && moduleBase_)
    {
      SymUnloadModule64(process_, moduleBase_);
    }
  }
  DWORD64 Get()
  {
    return moduleBase_;
  }
private:
  HANDLE process_;
  DWORD64 moduleBase_;
};

///////////////

const wchar_t *AdvanceString(const wchar_t *s)
{
  return s + wcslen(s) + 1;
}

const wchar_t *AdvanceStringUntilAfter(const wchar_t *s, const wchar_t *end, const wchar_t *match)
{
  do
  {
    s = AdvanceString(s);
  } while(s < end && wcscmp(s, match) != 0);
  return s < end ? AdvanceString(s) : s;
}

ConfigMap SplitConfig(const wchar_t *&cur, const wchar_t *end)
{
  ConfigMap rv;
  auto vv = Split(cur, end, L' ');
  for(auto &&cv : vv)
  {
    auto cfg = Split(cv, L'=');
    rv.emplace(cfg[0], cfg[1]);
  }
  return rv;
}

bool ParseFormatStr(const wchar_t *&fmtLine, const wchar_t *endFmtLine, std::wstring &fmtString)
{
  auto cur = fmtLine;
  //auto last = fmtLine + wcslen(fmtLine);
  auto first = std::find(cur, endFmtLine, L'\"');
  if(first == endFmtLine)
  {
    return false;
  }
  ++first;
  std::reverse_iterator<const wchar_t *> begin(endFmtLine);
  std::reverse_iterator<const wchar_t *> end(first);
  auto final = std::find(begin, end, L'\"');
  if(final == end)
  {
    return false;
  }
  ++final;
  auto endOfString = final.base();
  fmtString.assign(first, endOfString);
  fmtLine = endOfString + 2;
  return true;
}

bool ParseFormatLine(const wchar_t *fmtLine, std::wstring &fmtString, ConfigMap &cfgMap, DWORD &traceIndex)
{
  auto cur = fmtLine;
  auto last = fmtLine + wcslen(fmtLine);
  // skip #typev
  SkipUntil(cur, last, L' ');
  Skip(cur, last, L' ');
  // skip <id>
  SkipUntil(cur, last, L' ');
  Skip(cur, last, L' ');
  // get trace index
  traceIndex = wcstoul(cur, nullptr, 10);
  if(!ParseFormatStr(cur, last, fmtString))
  {
    return false;
  }
  Skip(cur, last, L' ');
  Skip(cur, last, L'/');
  Skip(cur, last, L' ');
  cfgMap = SplitConfig(cur, last);
  return true;
}

BOOL CALLBACK Check64BitEnumerator(PSYMBOL_INFOW pSymInfo, ULONG SymbolSize, PVOID UserContext)
{
	auto context = reinterpret_cast<TraceContext *>(UserContext);
	if (pSymInfo->Name == NULL ||
		pSymInfo->Size != 8 ||
		pSymInfo->Tag != SymTagTypedef)
	{
		return TRUE;
	}
	if (wcscmp(pSymInfo->Name, L"PVOID") == 0 ||
      wcscmp(pSymInfo->Name, L"ULONG_PTR") == 0 ||
      wcscmp(pSymInfo->Name, L"DWORD_PTR") == 0 ||
      wcscmp(pSymInfo->Name, L"UINT_PTR") == 0 ||
      wcscmp(pSymInfo->Name, L"PBYTE") == 0 ||
      wcscmp(pSymInfo->Name, L"HANDLE") == 0)
	{
		context->is64Bit = true;
		return FALSE;
	}
	return TRUE;
}

BOOL CALLBACK TraceLogEnumerator(PSYMBOL_INFOW pSymInfo, ULONG SymbolSize, PVOID UserContext)
{
	if (pSymInfo->Name == NULL)
	{
		return TRUE;
	}
  auto context = reinterpret_cast<TraceContext *>(UserContext);
  const wchar_t *name = pSymInfo->Name;
  const auto endName = name + pSymInfo->NameLen;
  if(wcscmp(name, L"TMC:") == 0)
  {
    auto guid = AdvanceString(name);
    std::wcout << L"{" << guid << L"}" << std::endl;
    //GuidFromString(guid, &context->moduleGuid);
    auto guidName = AdvanceString(guid);
    std::wcout << guidName << std::endl;
    auto tmp = AdvanceString(guidName);
    while(tmp < endName && wcslen(tmp) > 0)
    {
      std::wcout << tmp << std::endl;
      tmp = AdvanceString(tmp);
    }
  }
  else if(wcscmp(name, L"TMF:") == 0)
  {
    TraceFormat traceFormat;
    const size_t MAX_IDENTIFIER_LEN = 1024;
    std::vector<std::uint8_t> buffer(sizeof(SYMBOL_INFOW) + MAX_IDENTIFIER_LEN * sizeof(wchar_t));
    SYMBOL_INFOW *functionSymbol = reinterpret_cast<SYMBOL_INFOW *>(&buffer[0]);
    functionSymbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
    functionSymbol->MaxNameLen = MAX_IDENTIFIER_LEN;

    DWORD64 functionDisplacement = 0;
    BOOL functionOk = SymFromAddrW(context->process, pSymInfo->Address, &functionDisplacement, functionSymbol);
    DWORD lineDisplacement = 0;
    IMAGEHLP_LINEW64 lineInfo = {0};
    lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINEW64);
    BOOL lineOk = SymGetLineFromAddrW64(context->process, pSymInfo->Address, &lineDisplacement, &lineInfo);

    traceFormat.function = functionSymbol->Name;
    traceFormat.lineNumber = lineInfo.LineNumber;
    traceFormat.fileInfoFlags = context->is64Bit ? FIF_64BIT_TRACE : FIF_32BIT_TRACE;

    //traceFormat.sourceFilename = lineInfo.FileName;

    //std::wcout << "LOGGING IN: " << functionSymbol->Name << L" (" << lineInfo.FileName << L"@" << lineInfo.LineNumber << L")" << std::endl;
    auto modInfo = AdvanceString(pSymInfo->Name);
    //std::wcout << L"MODINFO: " << modInfo << std::endl;
    auto modData = Split(modInfo, nullptr, L' ');
    GUID fileGuid;
    GuidFromString(modData[0], &fileGuid);
    //
    //auto &sourceFile = context->sourceFiles[traceFormat.sourceFileGuid];
    //if(sourceFile.filePath.empty())
    //{
    //  sourceFile.filePath = lineInfo.FileName;
    //}
    //
    traceFormat.moduleName = modData[1];
    auto fmtInfo = AdvanceString(modInfo);
    //std::wcout << L"FMTINFO: " << fmtInfo << std::endl;
    ParseFormatLine(fmtInfo, traceFormat.formatString, traceFormat.traceCfg, traceFormat.traceIndex);
    auto str = AdvanceStringUntilAfter(fmtInfo, endName, L"{");
    while(str < endName && wcscmp(str, L"}") != 0)
    {
      //std::wcout << L"  \"" << str << L"\"" << std::endl;
      auto typeInfo = Split(str, endName, L", ");
      TypeValue tv;
      tv.argName = typeInfo[0];
      auto tid = Split(typeInfo[1], L" -- ");
      tv.type = tid[0];
      traceFormat.typeMap.emplace(std::wcstoul(tid[1].c_str(), nullptr, 10), std::move(tv));
      str = AdvanceString(str);
    }
    context->addFmt(fileGuid, lineInfo.FileName, std::move(traceFormat));
    //if(context->collection->AddSourceFileTrace(fileGuid, traceFormat))
    //{
    //  context->collection->SetSourceFilePath(fileGuid, lineInfo.FileName);
    //}
    //sourceFile.traceEvents[traceFormat.traceIndex] = traceFormat;
    //context->traceEvents.emplace(std::make_pair(traceFormat.sourceFileGuid, traceFormat.traceIndex), traceFormat);
  }
  else
  {
    std::wcout << "LOG: " << name << std::endl;
  }
	return TRUE;
}

} // private namespace

///////////////////

PdbProvider::PdbProvider()
  : process_(GetCurrentProcess())
{
  SymInitializeW(process_, L"", FALSE);
}
PdbProvider::~PdbProvider()
{
  SymCleanup(process_);
}

ProviderCallback PdbProvider::Provide(const fs::path &pdbPath) const
{
  return [this, pdbPath](const TraceFormatAddCallback &adder)
  {
	  std::error_code errorCode;
	  const auto fileSize = fs::file_size(pdbPath, errorCode);
	  if (errorCode)
	  {
		  return false;
	  }
    std::vector<std::uint8_t> fileData(fileSize);
	  AutoSymModule moduleBase(process_, SymLoadModuleExW(process_, NULL, pdbPath.c_str(), pdbPath.stem().c_str(), reinterpret_cast<DWORD64>(&fileData[0]), fileData.size(), nullptr, 0));
	  if (moduleBase.Get() == 0)
	  {
		  return false;
	  }
    TraceContext context;
    //context.collection = &collection_;
    context.addFmt = adder;
    context.process = process_;
	  if(!SymEnumTypesW(process_, moduleBase.Get(), Check64BitEnumerator, &context))
    {
      return false;
    }
	  if(!SymSearchW(process_, moduleBase.Get(), 0, SymTagAnnotation, nullptr, 0, TraceLogEnumerator, &context, SYMSEARCH_RECURSE))
    {
      return false;
    }
	  return true;
  };
}

} // namespace etl_lib

