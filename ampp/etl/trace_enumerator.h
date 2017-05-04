#pragma once
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

namespace etl_lib
{

class FormatDatabase
{
public:
  class Impl;
public:
  FormatDatabase();
  ~FormatDatabase();
  void AddProvider(const ProviderCallback &prov);
  void AddObserver(Observer *o) const;
  void RemoveObserver(Observer *o) const;
  const TraceFormat *FindTrace(const GUID &fileGuid, DWORD traceIdx) const;
  fs::path GetSourceFile(const GUID &fileGuid) const;
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
  const wchar_t *GetItemValue(size_t index, TraceEventDataItem item, size_t *valueLength) const;
  const std::wstring &GetItemValue(size_t index, TraceEventDataItem item) const;
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

} // namespace etl_lib

