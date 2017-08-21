#include "stdafx.h"
#include <ampp/etl/pdb_file.h>
#include <DbgHelp.h>
#include <ampp/etl/guid_util.h>
#include "string_util.h"

#pragma comment(lib, "dbghelp.lib")

namespace etl
{

class PdbSymbolAccessor
{
public:
  static PdbSymbol Create(HANDLE proc, PSYMBOL_INFOW pSymInfo, ULONG SymbolSize);
};

namespace
{

struct PdbEnumeratorContext
{
  HANDLE process;
  PdbEnumCallback enumFun;
};

//////////////////

BOOL CALLBACK PdbEnumerator(PSYMBOL_INFOW pSymInfo, ULONG SymbolSize, PVOID UserContext)
{
  if (pSymInfo->Name == NULL)
  {
    return TRUE;
  }
  auto context = reinterpret_cast<PdbEnumeratorContext *>(UserContext);
  return context->enumFun(PdbSymbolAccessor::Create(context->process, pSymInfo, SymbolSize)) ? TRUE : FALSE;
}

const wchar_t *AdvanceString(const wchar_t *s)
{
  return s + wcslen(s) + 1;
}

} // private namespace

class PdbSymbol::Impl
{
public:
  Impl(HANDLE proc, PSYMBOL_INFOW sym, ULONG l);
  const wchar_t *Descriptor() const;
  size_t DescriptorLength() const;
  size_t Size() const;
  ULONG64 Address() const;
  const wchar_t *Name() const;
  DWORD LineNumber() const;
  const wchar_t *SourceFile() const;
  DWORD Tag() const;
private:
  bool CacheSymbolData() const;
private:
  HANDLE process_;
  PSYMBOL_INFOW symbol_;
  ULONG symbolSize_;
  //
  mutable DWORD64 symbolDisplacement_;
  mutable std::vector<uint8_t> infoBuffer_;
  mutable DWORD lineDisplacement_;
  mutable IMAGEHLP_LINEW64 lineInfo_;
};

PdbSymbol PdbSymbolAccessor::Create(HANDLE proc, PSYMBOL_INFOW pSymInfo, ULONG SymbolSize)
{
  return PdbSymbol(std::make_unique<PdbSymbol::Impl>(proc, pSymInfo, SymbolSize));
}

PdbSymbol::Impl::Impl(HANDLE proc, PSYMBOL_INFOW sym, ULONG l)
  : process_(proc)
  , symbol_(sym)
  , symbolSize_(l)
  , symbolDisplacement_(0)
  , lineDisplacement_(0)
{
}

const wchar_t *PdbSymbol::Impl::Descriptor() const
{
  return symbol_->Name;
}

size_t PdbSymbol::Impl::DescriptorLength() const
{
  return symbol_->NameLen;
}

size_t PdbSymbol::Impl::Size() const
{
  return symbolSize_;
}

ULONG64 PdbSymbol::Impl::Address() const
{
  return symbol_->Address;
}

const wchar_t *PdbSymbol::Impl::Name() const
{
  if (!CacheSymbolData())
  {
    return nullptr;
  }
  SYMBOL_INFOW *functionSymbol = reinterpret_cast<SYMBOL_INFOW *>(&infoBuffer_[0]);
  return functionSymbol->Name;
}

DWORD PdbSymbol::Impl::LineNumber() const
{
  if (!CacheSymbolData())
  {
    return 0;
  }
  return lineInfo_.LineNumber;
}

const wchar_t *PdbSymbol::Impl::SourceFile() const
{
  if (!CacheSymbolData())
  {
    return nullptr;
  }
  return lineInfo_.FileName;
}

DWORD PdbSymbol::Impl::Tag() const
{
  return symbol_->Tag;
}

bool PdbSymbol::Impl::CacheSymbolData() const
{
  if (!infoBuffer_.empty())
  {
    return true;
  }
  const size_t MAX_IDENTIFIER_LEN = 1024;
  infoBuffer_.resize(sizeof(SYMBOL_INFOW) + MAX_IDENTIFIER_LEN * sizeof(wchar_t));
  SYMBOL_INFOW *functionSymbol = reinterpret_cast<SYMBOL_INFOW *>(&infoBuffer_[0]);
  functionSymbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
  functionSymbol->MaxNameLen = MAX_IDENTIFIER_LEN;

  if (!SymFromAddrW(process_, symbol_->Address, &symbolDisplacement_, functionSymbol))
  {
    infoBuffer_.clear();
    return false;
  }

  ZeroMemory(&lineInfo_, sizeof(lineInfo_));
  lineInfo_.SizeOfStruct = sizeof(IMAGEHLP_LINEW64);
  if (!SymGetLineFromAddrW64(process_, symbol_->Address, &lineDisplacement_, &lineInfo_))
  {
    infoBuffer_.clear();
    return false;
  }
  return true;
}

/////////////

PdbSymbol::PdbSymbol(std::unique_ptr<Impl> &&impl)
  : impl_(std::move(impl))
{
}

const wchar_t *PdbSymbol::Descriptor() const
{
  return impl_->Descriptor();
}

size_t PdbSymbol::DescriptorLength() const
{
  return impl_->DescriptorLength();
}

size_t PdbSymbol::Size() const
{
  return impl_->Size();
}

ULONG64 PdbSymbol::Address() const
{
  return impl_->Address();
}

const wchar_t *PdbSymbol::Name() const
{
  return impl_->Name();
}

DWORD PdbSymbol::LineNumber() const
{
  return impl_->LineNumber();
}

const wchar_t *PdbSymbol::SourceFile() const
{
  return impl_->SourceFile();
}

DWORD PdbSymbol::Tag() const
{
  return impl_->Tag();
}

//////////////////////////////

PdbFile::PdbFile()
  : process_(GetCurrentProcess())
  , moduleBase_(0)
  , arch_(Architecture::Unknown)
{
}

PdbFile::~PdbFile()
{
  Close();
}

bool PdbFile::Open(const fs::path &pdbPath)
{
  std::error_code errorCode;
  const auto fileSize = fs::file_size(pdbPath, errorCode);
  if (errorCode)
  {
    return false;
  }
  fileData_.resize(fileSize);
  moduleBase_ = SymLoadModuleExW(process_,
                                 NULL,
                                 pdbPath.c_str(),
                                 pdbPath.stem().c_str(),
                                 reinterpret_cast<DWORD64>(&fileData_[0]),
                                 static_cast<DWORD>(fileData_.size()),
                                 nullptr,
                                 0);
  if (moduleBase_ == 0)
  {
    return false;
  }

  bool x64 = false;
  EnumerateTypes([&x64](const PdbSymbol &sym) -> bool
  {
    if (sym.Size() != 8 || sym.Tag() != SymTagTypedef)
    {
      return true;
    }
    if (wcscmp(sym.Descriptor(), L"PVOID") == 0 ||
        wcscmp(sym.Descriptor(), L"ULONG_PTR") == 0 ||
        wcscmp(sym.Descriptor(), L"DWORD_PTR") == 0 ||
        wcscmp(sym.Descriptor(), L"UINT_PTR") == 0 ||
        wcscmp(sym.Descriptor(), L"PBYTE") == 0 ||
        wcscmp(sym.Descriptor(), L"HANDLE") == 0)
    {
      x64 = true;
      return false;
    }
    return true;
  });
  arch_ = x64 ? Architecture::X64 : Architecture::X86;
  return true;
}

void PdbFile::Close()
{
  if (moduleBase_ != 0)
  {
    SymUnloadModule64(process_, moduleBase_);
    moduleBase_ = 0;
  }
  fileData_.clear();
}

std::vector<GUID> PdbFile::GetTraceProviderGuids() const
{
  std::vector<GUID> traceGuids;
  EnumerateSymbols(SymTagAnnotation, [&traceGuids](const PdbSymbol &sym) -> bool {
    const wchar_t *name = sym.Descriptor();
    const auto endName = name + sym.DescriptorLength();
    if (wcscmp(name, L"TMC:") == 0)
    {
      auto guid = AdvanceString(name);
      GUID fileGuid;
      if (GuidFromString(guid, &fileGuid))
      {
        traceGuids.push_back(fileGuid);
      }
      auto guidName = AdvanceString(guid);
      //std::wcout << guidName << std::endl;
      auto tmp = AdvanceString(guidName);
      while (tmp < endName && wcslen(tmp) > 0)
      {
        //std::wcout << tmp << std::endl;
        tmp = AdvanceString(tmp);
      }
    }
    return true;
  });
  return traceGuids;
}

std::vector<GUID> PdbFile::GetSourceFileGuids() const
{
  std::set<GUID, GuidLess> sourceFileGuids;
  EnumerateSymbols(SymTagAnnotation, [&sourceFileGuids](const PdbSymbol &sym) -> bool {
    const wchar_t *name = sym.Descriptor();
    const auto endName = name + sym.DescriptorLength();
    if (wcscmp(name, L"TMF:") == 0)
    {
      auto modInfo = AdvanceString(name);
      auto modData = Split<wchar_t>(modInfo, nullptr, L' ');
      GUID fileGuid;
      if (GuidFromString(modData[0], &fileGuid))
      {
        sourceFileGuids.insert(fileGuid);
      }
    }
    return true;
  });
  return std::vector<GUID>(sourceFileGuids.begin(), sourceFileGuids.end());
}

bool PdbFile::EnumerateSymbols(DWORD symTag, const PdbEnumCallback &fun) const
{
  PdbEnumeratorContext context;
  context.enumFun = fun;
  context.process = process_;
  if (!SymSearchW(process_, moduleBase_, 0, symTag, nullptr, 0, PdbEnumerator, &context, SYMSEARCH_RECURSE))
  {
    return false;
  }
  return true;
}

bool PdbFile::EnumerateTypes(const PdbEnumCallback &fun) const
{
  PdbEnumeratorContext context;
  context.enumFun = fun;
  context.process = process_;
  if (!SymEnumTypesW(process_, moduleBase_, PdbEnumerator, &context))
  {
    return false;
  }
  return true;
}

bool PdbFile::EnumerateAnnotations(const PdbEnumCallback &fun) const
{
  return EnumerateSymbols(SymTagAnnotation, fun);
}

Architecture PdbFile::GetArchitecture() const
{
  return arch_;
}

/////////////////////

PdbFileManager::PdbFileManager()
  : process_(GetCurrentProcess())
{
  SymInitializeW(process_, L"", FALSE);
}

PdbFileManager::~PdbFileManager()
{
  SymCleanup(process_);
}

std::unique_ptr<PdbFile> PdbFileManager::LoadPdb(const fs::path &pdbPath) const
{
  std::unique_ptr<PdbFile> rv(new PdbFile);
  if (!rv->Open(pdbPath))
  {
    rv.reset();
  }
  return rv;
}

HANDLE PdbFileManager::GetSymbolHandle() const
{
  return process_;
}

} // namespace etl
