#pragma once

namespace etl
{

enum class Architecture
{
  Unknown,
  X86,
  X64
};

class PdbSymbol
{
  friend class PdbSymbolAccessor;
  class Impl;
public:
  const wchar_t *Descriptor() const;
  size_t DescriptorLength() const;
  size_t Size() const;
  ULONG64 Address() const;
  const wchar_t *Name() const;
  DWORD LineNumber() const;
  const wchar_t *SourceFile() const;
  DWORD Tag() const;
private:
  PdbSymbol(std::unique_ptr<Impl> &&impl);
private:
  std::unique_ptr<Impl> impl_;
};

using PdbEnumCallback = std::function<bool(const PdbSymbol &sym)>;

class PdbFileManager;

class PdbFile
{
  friend class PdbFileManager;
public:
  ~PdbFile();
  void Close();
  std::vector<GUID> GetTraceProviderGuids() const;
  std::vector<GUID> GetSourceFileGuids() const;
  bool EnumerateSymbols(DWORD symTag, const PdbEnumCallback &fun) const;
  bool EnumerateTypes(const PdbEnumCallback &fun) const;
  bool EnumerateAnnotations(const PdbEnumCallback &fun) const;
  Architecture GetArchitecture() const;
private:
  PdbFile();
  bool Open(const fs::path &pdbPath);
private:
  HANDLE process_;
  DWORD64 moduleBase_;
  std::vector<uint8_t> fileData_;
  Architecture arch_;
};

class PdbFileManager
{
public:
  PdbFileManager();
  ~PdbFileManager();
  std::unique_ptr<PdbFile> LoadPdb(const fs::path &pdbPath) const;
  HANDLE GetSymbolHandle() const;
private:
  HANDLE process_;
};

} // namespace etl

