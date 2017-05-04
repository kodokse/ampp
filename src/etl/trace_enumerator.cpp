#include "stdafx.h"
#include <ampp/etl/trace_enumerator.h>
#include <ampp/etl/time_util.h>
#include <ampp/etl/guid_util.h>
#include "trace_format_data_impl.h"
#include "trace_util.h"
#include "trace_format_impl.h"

#define INITGUID
#include <Evntrace.h>

using namespace std::string_literals;

Observable::Observable() {}

void Observable::AddObserver(Observer *o) const
{
  if(o)
  {
    observers_.push_back(o);
  }
}

void Observable::RemoveObserver(Observer *o) const
{
  if(o)
  {
    observers_.remove(o);
  }
}

void Observable::NotifyObservers() const
{
  auto it = observers_.begin();
  while(it != observers_.end())
  {
    auto tmp = it++;
    if(*tmp)
    {
      (*tmp)->Notify(this);
    }
  }
}


namespace etl_lib
{

#pragma pack(push, 1)
struct MOF_DATA
{
  GUID  sourceFileGUID;
  FILETIME timeStamp;
  DWORD threadId;
  DWORD processId;
  BYTE  *params;
};
#pragma pack(pop)

struct SourceFile
{
  std::map<DWORD, std::unique_ptr<TraceFormat>> traceEvents;
  fs::path filePath;
  //std::map<std::wstring, DWORD> traceBits;
  //std::wstring moduleName;
};

class FormatDatabase::Impl : public Observable
{
public:
  bool AddSourceFileTrace(const GUID &fileGuid, const TraceFormat &fmt);
  void SetSourceFilePath(const GUID &fileGuid, const fs::path &filePath);
  const TraceFormat *FindTrace(const GUID &fileGuid, DWORD traceIdx) const;
  fs::path GetSourceFile(const GUID &fileGuid) const;
private:
  GuidKeyedMap<std::unique_ptr<SourceFile>> sourceFiles_;
};


//////////////////////////////

bool FormatDatabase::Impl::AddSourceFileTrace(const GUID &fileGuid, const TraceFormat &fmt)
{
  auto it = sourceFiles_.emplace(fileGuid, std::make_unique<SourceFile>());
  it.first->second->traceEvents[fmt.traceIndex] = std::make_unique<TraceFormat>(fmt);
  return it.second;
}

void FormatDatabase::Impl::SetSourceFilePath(const GUID &fileGuid, const fs::path &filePath)
{
  sourceFiles_[fileGuid]->filePath = filePath;
}

const TraceFormat *FormatDatabase::Impl::FindTrace(const GUID &fileGuid, DWORD traceIdx) const
{
  auto sit = sourceFiles_.find(fileGuid);
  if(sit == sourceFiles_.end())
  {
    return nullptr;
  }
  auto it = sit->second->traceEvents.find(traceIdx);
  if(it == sit->second->traceEvents.end())
  {
    return nullptr;
  }
  return it->second.get();
}

fs::path FormatDatabase::Impl::GetSourceFile(const GUID &fileGuid) const
{
  auto sit = sourceFiles_.find(fileGuid);
  if(sit == sourceFiles_.end())
  {
    return fs::path();
  }
  return sit->second->filePath;
}

////////////////////////////////////

FormatDatabase::FormatDatabase()
  : impl_(std::make_unique<FormatDatabase::Impl>())
{
}

FormatDatabase::~FormatDatabase(){}

void FormatDatabase::AddProvider(const ProviderCallback &prov)
{
  if(prov([this](const GUID &fileGuid, PWSTR fileName, TraceFormat &&traceFormat)
  {
    if(impl_->AddSourceFileTrace(fileGuid, std::move(traceFormat)))
    {
      impl_->SetSourceFilePath(fileGuid, fileName);
    }
  }))
  {
    impl_->NotifyObservers();
  }
}

void FormatDatabase::AddObserver(Observer *o) const
{
  impl_->AddObserver(o);
}

void FormatDatabase::RemoveObserver(Observer *o) const
{
  impl_->RemoveObserver(o);
}

const TraceFormat *FormatDatabase::FindTrace(const GUID &fileGuid, DWORD traceIdx) const
{
  return impl_->FindTrace(fileGuid, traceIdx);
}

fs::path FormatDatabase::GetSourceFile(const GUID &fileGuid) const
{
  return impl_->GetSourceFile(fileGuid);
}

///////////////////////////////////////////

enum class DataState
{
  NoData,
  NoProvider,
  HasData
};

struct TraceEventItem
{
  GUID fileGuid;
  DWORD traceIdx;
  std::vector<std::uint8_t> paramData;
  DataState dataState;
  //
  std::vector<std::wstring> values;
  //
  std::wstring &operator[](TraceEventDataItem item)
  {
    return values.at(static_cast<size_t>(item));
  }
  const std::wstring &operator[](TraceEventDataItem item) const
  {
    return values.at(static_cast<size_t>(item));
  }
  //
  TraceEventItem(const GUID &guid, const FILETIME &ts, DWORD trid, DWORD pid, DWORD tid, DWORD order, PVOID d, ULONG dl)
    : fileGuid(guid)
    , traceIdx(trid)
    , paramData(reinterpret_cast<std::uint8_t *>(d), reinterpret_cast<std::uint8_t *>(d) + dl)
    , dataState(DataState::NoData)
  {
    values.resize(static_cast<size_t>(TraceEventDataItem::MAX_ITEM));
    (*this)[TraceEventDataItem::TimeStamp] = FormatFileTime(L"Y-M-D H:m:S.l", ts);
    (*this)[TraceEventDataItem::ProcessId] = std::to_wstring(pid);
    (*this)[TraceEventDataItem::ThreadId] = std::to_wstring(tid);
    (*this)[TraceEventDataItem::TraceIndex] = std::to_wstring(order);
  }
  bool EvaluateItem(const FormatDatabase &db)
  {
    if(dataState == DataState::HasData)
    {
      return true;
    }
    if(dataState == DataState::NoProvider)
    {
      return false;
    }
    auto traceFmt = db.FindTrace(fileGuid, traceIdx);
    if(!traceFmt)
    {
      dataState = DataState::NoProvider;
      return false;
    }
    TraceFormatData fmtData {&paramData[0], paramData.size()};
    (*this)[TraceEventDataItem::Function] = traceFmt->function;
    (*this)[TraceEventDataItem::ModuleName] = traceFmt->moduleName;
    (*this)[TraceEventDataItem::LineNumber] = std::to_wstring(traceFmt->lineNumber);
    (*this)[TraceEventDataItem::SourceFile] = db.GetSourceFile(fileGuid);
    (*this)[TraceEventDataItem::Message] = FormatTraceFormat(*traceFmt, fmtData);
    (*this).dataState = DataState::HasData;
    return true;
  }
};

class TraceEnumerator::Impl : public Observer
{
  friend class TraceEnumerator;
public:
  TraceEnumerator::Impl::Impl(const FormatDatabase *db);
  TraceEnumerator::Impl::~Impl();
  void SetStartTime(const FILETIME &startTime);
  void SetStartTime(const LARGE_INTEGER &startTime);
  size_t GetItemCount() const;
  const wchar_t *GetItemValue(size_t index, TraceEventDataItem item, size_t *valueLength) const;
  const std::wstring &GetItemValue(size_t index, TraceEventDataItem item) const;
  void ApplyFilters();
  static void CALLBACK EventCallback(PEVENT_TRACE pEvent);
  //
  void Notify(const Observable *o) override;
private:
  bool GenerateLogEntry(const GUID &fileGuid, DWORD traceId, const FILETIME &timeStamp, DWORD processId, DWORD threadId, PVOID params, size_t paramLen);
  bool TestFilters(TraceEventItem &tei);
  bool EvaluateItem(TraceEventItem &tei) const;
private:
  const FormatDatabase *db_;
  LogCallback logger_;
  CountCallback countCallback_;
  FILETIME startTime_;
  std::vector<std::unique_ptr<TraceEventItem>> allTraces_;
  std::vector<TraceEventItem *> filteredTraceEvents_;
  std::vector<std::function<bool (TraceEventDataItem item, const std::wstring &value)>> filters_;
};

TraceEnumerator::Impl::Impl(const FormatDatabase *db)
  : db_(db)
{
  db_->AddObserver(this);
}

TraceEnumerator::Impl::~Impl()
{
  db_->RemoveObserver(this);
}

void TraceEnumerator::Impl::Notify(const Observable *o)
{
  for(auto &&te : allTraces_)
  {
    if(te->dataState == DataState::NoProvider)
    {
      te->dataState = DataState::NoData;
    }
  }
}


thread_local TraceEnumerator::Impl *g_thread_enum = nullptr;

TraceEnumerator::TraceEnumerator(const FormatDatabase &db)
  : impl_(std::make_unique<Impl>(&db))
{
}

TraceEnumerator::~TraceEnumerator()
{
}

void TraceEnumerator::SetLogFunction(const LogCallback &logger)
{
  impl_->logger_ = logger;
}

void TraceEnumerator::SetCountCallback(const CountCallback &countCallback)
{
  impl_->countCallback_ = countCallback;
}

void TraceEnumerator::AddFilter(const std::function<bool (TraceEventDataItem item, const std::wstring &txt)> &filter)
{
  impl_->filters_.push_back(filter);
  impl_->ApplyFilters();
}

void TraceEnumerator::RemoveAllFilters()
{
  impl_->filters_.clear();
  impl_->ApplyFilters();
}

size_t TraceEnumerator::GetItemCount() const
{
  return impl_->GetItemCount();
}

const wchar_t *TraceEnumerator::GetItemValue(size_t index, TraceEventDataItem item, size_t *valueLength) const
{
  return impl_->GetItemValue(index, item, valueLength);
}

const std::wstring &TraceEnumerator::GetItemValue(size_t index, TraceEventDataItem item) const
{
  return impl_->GetItemValue(index, item);
}

void TraceEnumerator::ApplyFilters()
{
  impl_->ApplyFilters();
}

////////////////////

bool TraceEnumerator::Impl::GenerateLogEntry(const GUID &fileGuid, DWORD traceId, const FILETIME &timeStamp, DWORD processId, DWORD threadId, PVOID params, size_t paramLen)
{
  auto traceFmt = db_->FindTrace(fileGuid, traceId);
  if(!traceFmt)
  {
    return false;
  }
  TraceFormatData fmtData {params, paramLen};
  TraceEventData evt;
  evt.function = traceFmt->function;
  evt.moduleName = traceFmt->moduleName;
  evt.lineNumber = traceFmt->lineNumber;
  evt.sourceFile = db_->GetSourceFile(fileGuid);
  evt.processId = processId;
  evt.threadId = threadId;
  evt.timeStamp = startTime_ + timeStamp;
  evt.message = FormatTraceFormat(*traceFmt, fmtData);
  logger_(evt);
  return true;
}

bool TraceEnumerator::Impl::TestFilters(TraceEventItem &tei)
{
  tei.EvaluateItem(*db_);
  for(auto &&f : filters_)
  {
    for(auto v = TraceEventDataItem::TraceIndex; v < TraceEventDataItem::MAX_ITEM; ++v)
    {
      if(!f(v, tei[v]))
      {
        return false;
      }
    }
  }
  return true;
}

void TraceEnumerator::Impl::ApplyFilters()
{
  filteredTraceEvents_.clear();
  for(auto &&te : allTraces_)
  {
    if(TestFilters(*te))
    {
      filteredTraceEvents_.push_back(te.get());
    }
  }
  if(countCallback_)
  {
    countCallback_(filteredTraceEvents_.size());
  }
}

void CALLBACK TraceEnumerator::Impl::EventCallback(PEVENT_TRACE pEvent)
{
  if(pEvent->MofData == NULL)
  {
    return;
  }
  if(pEvent->Header.Guid == EventTraceGuid)
  {
    return;
  }
  auto context = g_thread_enum;
  if(!context)
  {
    return;
  }
  auto mofData = reinterpret_cast<MOF_DATA *>(pEvent->MofData);
  DWORD traceId = LOWORD(pEvent->Header.Version);
  context->allTraces_.push_back(std::make_unique<TraceEventItem>(mofData->sourceFileGUID, context->startTime_ + mofData->timeStamp, traceId, mofData->processId, mofData->threadId, context->allTraces_.size(), &mofData->params, pEvent->MofLength));
  context->EvaluateItem(*context->allTraces_.back());
  if(context->TestFilters(*context->allTraces_.back()))
  {
    context->filteredTraceEvents_.push_back(context->allTraces_.back().get());
  }
  if(context->countCallback_)
  {
    context->countCallback_(context->filteredTraceEvents_.size());
  }
}

bool TraceEnumerator::Impl::EvaluateItem(TraceEventItem &ev) const
{
  return ev.EvaluateItem(*db_);
}

void TraceEnumerator::Impl::SetStartTime(const FILETIME &startTime)
{
  startTime_ = startTime;
}

void TraceEnumerator::Impl::SetStartTime(const LARGE_INTEGER &startTime)
{
  startTime_.dwHighDateTime = startTime.HighPart;
  startTime_.dwLowDateTime = startTime.LowPart;
}

size_t TraceEnumerator::Impl::GetItemCount() const
{
  return filteredTraceEvents_.size();
}

const std::wstring &TraceEnumerator::Impl::GetItemValue(size_t index, TraceEventDataItem item) const
{
  static std::wstring EmptyString;
  if(index >= filteredTraceEvents_.size())
  {
    return EmptyString;
  }
  auto ev = filteredTraceEvents_[index];
  EvaluateItem(*ev);
  return (*ev)[item];
}

const wchar_t *TraceEnumerator::Impl::GetItemValue(size_t index, TraceEventDataItem item, size_t *valueLength) const
{
  if(index >= filteredTraceEvents_.size())
  {
    return nullptr;
  }
  auto ev = filteredTraceEvents_[index];
  EvaluateItem(*ev);
  if(valueLength)
  {
    *valueLength = (*ev)[item].length();
  }
  return (*ev)[item].c_str();
}


////////////////////

LogfileEnumerator::LogfileEnumerator(const FormatDatabase &db, const fs::path &logPath)
  : TraceEnumerator(db)
  , logPath_(logPath)
{
}

bool LogfileEnumerator::Start()
{
  EVENT_TRACE_LOGFILE traceFile;
  ZeroMemory(&traceFile, sizeof(traceFile));
  g_thread_enum = impl_.get();
  traceFile.EventCallback = Impl::EventCallback;
  traceFile.LogFileMode = EVENT_TRACE_FILE_MODE_NONE;
  traceFile.LogFileName = const_cast<LPWSTR>(logPath_.c_str());
  auto traceHandle = OpenTrace(&traceFile);
  if(traceHandle == (TRACEHANDLE)INVALID_HANDLE_VALUE)
  {
    return false;
  }
  impl_->SetStartTime(traceFile.LogfileHeader.StartTime);
  ProcessTrace(&traceHandle, 1, NULL, NULL);
  return true;
}

void LogfileEnumerator::Stop()
{
}

} // namespace etl_lib


