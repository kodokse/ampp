#include "stdafx.h"
#include <ampp/etl/pdb_provider.h>
#include <ampp/etl/guid_util.h>
#include <ampp/etl/pdb_file.h>
#include <ampp/etl/string_util.h>
#include "trace_format_impl.h"

namespace etl
{

namespace
{

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

} // private namespace


///////////////////

class PdbProvider::Impl
{
public:
  Impl(const PdbFileManager &fm, const fs::path &pdbPath);
  ~Impl();
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
}

PdbProvider::Impl::~Impl()
{
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
    auto modData = Split<wchar_t>(modInfo, nullptr, L' ');
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

bool PdbProvider::Impl::ProvideForPdbFile(const SourceFileCallback &sourceFileCb, const TraceGuidCallback &traceGuidCb, const fs::path &pdbPath) const
{
  auto pdbFile = fm_.LoadPdb(pdbPath);
  if(!pdbFile)
  {
    return false;
  }
  auto traceGuids = pdbFile->GetTraceProviderGuids();
  for (auto &&tguid : traceGuids)
  {
    traceGuidCb(tguid, nullptr);
  }
  auto guids = pdbFile->GetSourceFileGuids();
  for (auto &&guid : guids)
  {
    sourceFileCb(guid, [this, pdbPath](const TraceFormatAddCallback &adder)
    {
      auto pdbFile = fm_.LoadPdb(pdbPath);
      if (pdbFile)
      {
        pdbFile->EnumerateAnnotations([this, adder, arch = pdbFile->GetArchitecture()](const PdbSymbol &sym) -> bool
        {
          return EnumerateTrace(sym, arch, adder);
        });
      }
      return true;
    });
  }
  return true;
}

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

bool PdbProvider::EnumerateTraces(const SourceFileCallback &sourceFileCb, const TraceGuidCallback &traceGuidCb) const
{
  return impl_->EnumerateTraces(sourceFileCb, traceGuidCb);
}

} // namespace etl

