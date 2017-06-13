#pragma once
#include <Evntrace.h>
#include "trace_event_data.h"
#include "trace_base.h"

class Observable;

class Observer
{
public:
  virtual ~Observer() {}
  virtual void Notify(const Observable *o) = 0;
};

class Observable
{
public:
  Observable();
  void AddObserver(Observer *o) const;
  void RemoveObserver(Observer *o) const;
  void NotifyObservers() const;
private:
  mutable std::list<Observer *> observers_;
};

namespace etl
{

class FormatDatabase
{
public:
  class Impl;
public:
  FormatDatabase();
  ~FormatDatabase();
  void AddProvider(const TraceProvider &prov);
  void AddObserver(Observer *o) const;
  void RemoveObserver(Observer *o) const;
  const TraceFormat *FindTrace(const GUID &fileGuid, DWORD traceIdx) const;
  fs::path GetSourceFile(const GUID &fileGuid) const;
  std::vector<GUID> GetTraceProviderGuids() const;
private:
  std::unique_ptr<Impl> impl_;
};

class TraceEnumerator
{
public:
  class Impl;
  using LogCallback = std::function<void (const TraceEventData &evt)>;
  using CountCallback = std::function<void (size_t count)>;
public:
  TraceEnumerator(const FormatDatabase &db);
  ~TraceEnumerator();
  void SetLogFunction(const LogCallback &logger);
  void SetCountCallback(const CountCallback &countCallback);
  void AddFilter(const std::function<bool (TraceEventDataItem item, const std::wstring &txt)> &filter);
  void RemoveAllFilters();
  void ApplyFilters();
  size_t GetItemCount() const;
  void RemoveAllItems();
  const wchar_t *GetItemValue(size_t index, TraceEventDataItem item, size_t *valueLength) const;
  const std::wstring &GetItemValue(size_t index, TraceEventDataItem item) const;
  void *GetItemMetadata(size_t index);
  bool SetItemMetadata(size_t index, void *metadata);
  virtual bool Start() = 0;
  virtual void Stop() = 0;
protected:
  std::unique_ptr<Impl> impl_;
};

class LogfileEnumerator : public TraceEnumerator
{
public:
  LogfileEnumerator(const FormatDatabase &db, const fs::path &logPath);
  bool Start() override;
  void Stop() override;
private:
  fs::path logPath_;
};

class LiveTraceEnumerator : public TraceEnumerator
{
public:
  LiveTraceEnumerator(const FormatDatabase &db, const std::wstring &sessionName);
  bool Start() override;
  void Stop() override;
private:
  void InitSession();
  const wchar_t *GetSessionName() const;
private:
  std::wstring sessionName_;
  std::vector<char> traceBuffer_;
  TRACEHANDLE traceHandle_;
  std::unique_ptr<std::thread> processor_;
};

} // namespace etl

