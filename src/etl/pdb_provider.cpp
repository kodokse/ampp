#include "stdafx.h"
#include <DbgHelp.h>
#include <ampp/etl/pdb_provider.h>
#include <ampp/etl/guid_util.h>
#include <ampp/etl/pdb_file.h>
#include "string_util.h"
#include "trace_format_impl.h"

#pragma comment(lib, "dbghelp.lib")

namespace etl
{

namespace
{

using PdbEnumCallback = std::function<bool(PSYMBOL_INFOW pSymInfo, ULONG SymbolSize)>;

struct PdbEnumeratorContext
{
  PdbEnumCallback enumFun;
};

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

/*
BOOL CALLBACK Check64BitEnumerator(PSYMBOL_INFOW pSymInfo, ULONG SymbolSize, PVOID UserContext)
{
  auto context = reinterpret_cast<TraceContext *>(UserContext);
  if (pSymInfo->Name == NULL || pSymInfo->Size != 8 || pSymInfo->Tag != SymTagTypedef)
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

BOOL CALLBACK PdbEnumerator(PSYMBOL_INFOW pSymInfo, ULONG SymbolSize, PVOID UserContext)
{
  if (pSymInfo->Name == NULL)
  {
    return TRUE;
  }
  auto context = reinterpret_cast<PdbEnumeratorContext *>(UserContext);
  return context->enumFun(pSymInfo, SymbolSize) ? TRUE : FALSE;
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
*/

} // private namespace


///////////////////

class PdbProvider::Impl
{
public:
  Impl(const PdbFileManager &fm, const fs::path &pdbPath);
  ~Impl();
  //ProviderEnumerator Provide(const fs::path &pdbPath) const;
  bool EnumerateTraces(const SourceFileCallback &sourceFileCb, const TraceGuidCallback &traceGuidCb) const;
private:
  bool ProvideForPdbFile(const SourceFileCallback &sourceFileCb, const TraceGuidCallback &traceGuidCb, const fs::path &pdbPath) const;
private:
  bool EnumerateTrace(const PdbSymbol &sym, Architecture arch, const TraceFormatAddCallback &adder) const;
private:
  const PdbFileManager &fm_;
  fs::path pdbPath_;
};

//////////////////

PdbProvider::Impl::Impl(const PdbFileManager &fm, const fs::path &pdbPath)
  : fm_(fm)
  , pdbPath_(pdbPath)
{
  //SymInitializeW(process_, L"", FALSE);
}

PdbProvider::Impl::~Impl()
{
  //SymCleanup(process_);
}

bool PdbProvider::Impl::EnumerateTrace(const PdbSymbol &sym, Architecture arch, const TraceFormatAddCallback &adder) const
{
  const wchar_t *name = sym.Descriptor();
  const auto endName = name + sym.DescriptorLength();
  if (wcscmp(name, L"TMF:") == 0)
  {
    TraceFormat traceFormat;

    traceFormat.function = sym.Name();
    traceFormat.lineNumber = sym.LineNumber();
    traceFormat.fileInfoFlags = arch ==  Architecture::X64 ? FIF_64BIT_TRACE : FIF_32BIT_TRACE;

    auto modInfo = AdvanceString(name);
    auto modData = Split(modInfo, nullptr, L' ');
    GUID fileGuid;
    GuidFromString(modData[0], &fileGuid);
    traceFormat.moduleName = modData[1];
    auto fmtInfo = AdvanceString(modInfo);
    ParseFormatLine(fmtInfo, traceFormat.formatString, traceFormat.traceCfg, traceFormat.traceIndex);
    auto str = AdvanceStringUntilAfter(fmtInfo, endName, L"{");
    while (str < endName && wcscmp(str, L"}") != 0)
    {
      auto typeInfo = Split(str, endName, L", ");
      TypeValue tv;
      tv.argName = typeInfo[0];
      auto tid = Split(typeInfo[1], L" -- ");
      tv.type = tid[0];
      traceFormat.typeMap.emplace(std::wcstoul(tid[1].c_str(), nullptr, 10), std::move(tv));
      str = AdvanceString(str);
    }
    adder(fileGuid, sym.SourceFile(), std::move(traceFormat));
  }
  return true;
}

/*
bool PdbProvider::Impl::PdbEnumerate(const fs::path &pdbPath, DWORD symTag, const PdbEnumCallback &fun) const
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
  PdbEnumeratorContext context;
  context.enumFun = fun;
  if (!SymSearchW(process_, moduleBase.Get(), 0, symTag, nullptr, 0, PdbEnumerator, &context, SYMSEARCH_RECURSE))
  {
    return false;
  }
  return true;
}
*/

/*
std::vector<GUID> PdbProvider::Impl::EnumerateSourceFileGuids(const fs::path &pdbPath) const
{
  //std::vector<GUID> sourceFileGuids;
  //PdbEnumerate(pdbPath, SymTagAnnotation, [&sourceFileGuids](PSYMBOL_INFOW pSymInfo, ULONG SymbolSize) -> bool {
  //  const wchar_t *name = pSymInfo->Name;
  //  const auto endName = name + pSymInfo->NameLen;
  //  if (wcscmp(name, L"TMF:") == 0)
  //  {
  //    auto modInfo = AdvanceString(name);
  //    auto modData = Split(modInfo, nullptr, L' ');
  //    GUID fileGuid;
  //    if (GuidFromString(modData[0], &fileGuid))
  //    {
  //      sourceFileGuids.push_back(fileGuid);
  //    }
  //  }
  //  return true;
  //});
  return sourceFileGuids;
}
*/

bool PdbProvider::Impl::ProvideForPdbFile(const SourceFileCallback &sourceFileCb, const TraceGuidCallback &traceGuidCb, const fs::path &pdbPath) const
{
  std::wcout << L"Loading: " << pdbPath.filename() << L"..." << std::flush;
  auto pdbFile = fm_.LoadPdb(pdbPath);
  //if (!pdbFile.Open(pdbPath))
  if(!pdbFile)
  {
    std::wcout << L"FAIL!" << std::endl;
    return false;
  }
  auto traceGuids = pdbFile->GetTraceProviderGuids();
  for (auto &&tguid : traceGuids)
  {
    traceGuidCb(tguid, nullptr);
  }
  auto guids = pdbFile->GetSourceFileGuids(); // EnumerateSourceFileGuids(pdbPath);
  for (auto &&guid : guids)
  {
    //traceGuidCb(guid, nullptr);
    sourceFileCb(guid, [this, pdbPath](const TraceFormatAddCallback &adder)
    {
      auto pdbFile = fm_.LoadPdb(pdbPath);
      if (pdbFile)
      {
        pdbFile->EnumerateSymbols(SymTagAnnotation, [this, adder, arch = pdbFile->GetArchitecture()](const PdbSymbol &sym) -> bool
        {
          return EnumerateTrace(sym, arch, adder);
        });
      }
      return true;
    });
  }
  std::wcout << L"OK!" << std::endl;
  return true;
}

/*
ProviderEnumerator PdbProvider::Impl::Provide(const fs::path &pdbPath) const
{
  return [this, pdbPath](const ProviderEnumeratorCallback &enumCallback) {
    // for each PDB in path, get the source file GUIDs
    if (fs::is_regular_file(pdbPath))
    {
      ProvideForPdbFile(enumCallback, pdbPath);
    }
    else if (fs::is_directory(pdbPath))
    {
      fs::directory_iterator di(pdbPath);
      fs::directory_iterator end;
      while (di != end)
      {
        if (fs::is_regular_file(di->path()) && di->path().extension().wstring() == L".pdb")
        {
          ProvideForPdbFile(enumCallback, di->path());
        }
        ++di;
      }
    }
    // then for each source file GUID create a callback
  };
}
*/

bool PdbProvider::Impl::EnumerateTraces(const SourceFileCallback &sourceFileCb, const TraceGuidCallback &traceGuidCb) const
{
  // for each PDB in path, get the source file GUIDs
  if (fs::is_regular_file(pdbPath_))
  {
    return ProvideForPdbFile(sourceFileCb, traceGuidCb, pdbPath_);
  }
  else if (fs::is_directory(pdbPath_))
  {
    fs::directory_iterator di(pdbPath_);
    fs::directory_iterator end;
    bool rv = false;
    while (di != end)
    {
      if (fs::is_regular_file(di->path()) && di->path().extension().wstring() == L".pdb")
      {
        rv = ProvideForPdbFile(sourceFileCb, traceGuidCb, di->path()) || rv;
      }
      ++di;
    }
    return rv;
  }
  return false;
}

///////////////////////

PdbProvider::PdbProvider(const PdbFileManager &fm, const fs::path &pdbFile)
  : impl_(std::make_unique<PdbProvider::Impl>(fm, pdbFile))
{
}

PdbProvider::~PdbProvider()
{
}

//ProviderEnumerator PdbProvider::Provide(const fs::path &pdbPath) const
//{
//  return impl_->Provide(pdbPath);
//}

bool PdbProvider::EnumerateTraces(const SourceFileCallback &sourceFileCb, const TraceGuidCallback &traceGuidCb) const
{
  return impl_->EnumerateTraces(sourceFileCb, traceGuidCb);
}

} // namespace etl

