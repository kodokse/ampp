#include "stdafx.h"
#define INITGUID
#include <ampp/etl/trace_enumerator.h>
#include <ampp/etl/time_util.h>
#include <ampp/etl/guid_util.h>
#include "trace_format_data_impl.h"
#include "trace_util.h"
#include "trace_format_impl.h"

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


namespace etl
{

#pragma pack(push, 1)
struct MOF_DATA_FILE
{
  GUID  sourceFileGUID;
  FILETIME timeStamp;
  DWORD threadId;
  DWORD processId;
  BYTE  *params;
};
struct MOF_DATA_LIVE
{
  DWORD sequenceId;
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
  void AddProvider(const GUID &fileGuid, const ProviderCallback &provider);
  const TraceFormat *FindTrace(const GUID &fileGuid, DWORD traceIdx) const;
  fs::path GetSourceFile(const GUID &fileGuid) const;
  void AddTraceGuid(const GUID &traceGuid, const wchar_t *traceName);
  std::vector<GUID> GetTraceProviderGuids() const;
private:
  const TraceFormat *FindTraceInternal(const GUID &fileGuid, DWORD traceIdx) const;
  void InvokeProvider(const GUID &fileGuid) const;
  bool AddSourceFileTrace(const GUID &fileGuid, const TraceFormat &fmt) const;
  void SetSourceFilePath(const GUID &fileGuid, const fs::path &filePath) const;
private:
  mutable GuidKeyedMap<std::unique_ptr<SourceFile>> sourceFiles_;
  GuidKeyedMap<ProviderCallback> providers_;
  GuidKeyedMap<std::wstring> traces_;
};


//////////////////////////////

void FormatDatabase::Impl::AddProvider(const GUID &fileGuid, const ProviderCallback &provider)
{
  providers_[fileGuid] = provider;
  NotifyObservers();
}

bool FormatDatabase::Impl::AddSourceFileTrace(const GUID &fileGuid, const TraceFormat &fmt) const
{
  auto it = sourceFiles_.emplace(fileGuid, std::make_unique<SourceFile>());
  it.first->second->traceEvents[fmt.traceIndex] = std::make_unique<TraceFormat>(fmt);
  return it.second;
}

void FormatDatabase::Impl::SetSourceFilePath(const GUID &fileGuid, const fs::path &filePath) const
{
  sourceFiles_[fileGuid]->filePath = filePath;
}

const TraceFormat *FormatDatabase::Impl::FindTrace(const GUID &fileGuid, DWORD traceIdx) const
{
  auto rv = FindTraceInternal(fileGuid, traceIdx);
  if (rv)
  {
    return rv;
  }
  InvokeProvider(fileGuid);
  return FindTraceInternal(fileGuid, traceIdx);
}

void FormatDatabase::Impl::InvokeProvider(const GUID &fileGuid) const
{
  auto it = providers_.find(fileGuid);
  if (it != providers_.end())
  {
    if(it->second(
      [this](const GUID &fileGuid, const wchar_t *fileName, TraceFormat &&traceFormat)
      {
        if(AddSourceFileTrace(fileGuid, std::move(traceFormat)))
        {
          SetSourceFilePath(fileGuid, fileName);
        }
      }))
    {
      NotifyObservers();
    }
  }
}

const TraceFormat *FormatDatabase::Impl::FindTraceInternal(const GUID &fileGuid, DWORD traceIdx) const
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

void FormatDatabase::Impl::AddTraceGuid(const GUID &traceGuid, const wchar_t *traceName)
{
  traces_[traceGuid] = traceName ? traceName : L"";
}

std::vector<GUID> FormatDatabase::Impl::GetTraceProviderGuids() const
{
  std::vector<GUID> rv;
  for (auto &&kv : traces_)
  {
    rv.push_back(kv.first);
  }
  return rv;
}

////////////////////////////////////

FormatDatabase::FormatDatabase()
  : impl_(std::make_unique<FormatDatabase::Impl>())
{
}

FormatDatabase::~FormatDatabase(){}

void FormatDatabase::AddProvider(const TraceProvider &provEnum)
{
  provEnum.EnumerateTraces(
    [this](const GUID &fileGuid, const ProviderCallback &prov)
    {
      impl_->AddProvider(fileGuid, prov);
    },
    [this](const GUID &traceGuid, const wchar_t *traceName)
    {
      impl_->AddTraceGuid(traceGuid, traceName);
    });
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

std::vector<GUID> FormatDatabase::GetTraceProviderGuids() const
{
  return impl_->GetTraceProviderGuids();
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
  template <class MofType>
  static void CALLBACK EventCallback(PEVENT_TRACE pEvent);
  std::vector<GUID> GetTraceGuids() const;
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

std::vector<GUID> TraceEnumerator::Impl::GetTraceGuids() const
{
  return db_->GetTraceProviderGuids();
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

template <class MofType>
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
  auto mofData = reinterpret_cast<MofType *>(pEvent->MofData);
  DWORD traceId = LOWORD(pEvent->Header.Version);
  context->allTraces_.push_back(std::make_unique<TraceEventItem>(mofData->sourceFileGUID, context->startTime_ + mofData->timeStamp, traceId, mofData->processId, mofData->threadId, static_cast<DWORD>(context->allTraces_.size()), &mofData->params, pEvent->MofLength));
  context->EvaluateItem(*context->allTraces_.back());
  if(context->TestFilters(*context->allTraces_.back()))
  {
    context->filteredTraceEvents_.push_back(context->allTraces_.back().get());
  }
  if(context->countCallback_)
  {
    context->countCallback_(context->filteredTraceEvents_.size());
  }
  if (context->logger_)
  {
    context->GenerateLogEntry(mofData->sourceFileGUID, traceId, context->startTime_ + mofData->timeStamp, mofData->processId, mofData->threadId, &mofData->params, pEvent->MofLength);
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
  EVENT_TRACE_LOGFILEW traceFile;
  ZeroMemory(&traceFile, sizeof(traceFile));
  g_thread_enum = impl_.get();
  traceFile.EventCallback = Impl::EventCallback<MOF_DATA_FILE>;
  traceFile.LogFileMode = EVENT_TRACE_FILE_MODE_NONE;
  traceFile.LogFileName = const_cast<LPWSTR>(logPath_.c_str());
  auto traceHandle = OpenTraceW(&traceFile);
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

////////////////////////

LiveTraceEnumerator::LiveTraceEnumerator(const FormatDatabase &db, const std::wstring &sessionName)
  : TraceEnumerator(db)
  , sessionName_(sessionName)
{
}

void LiveTraceEnumerator::InitSession()
{
  traceBuffer_.resize((sessionName_.length() + 1) * sizeof(wchar_t) + sizeof(EVENT_TRACE_PROPERTIES));
  auto traceProps = reinterpret_cast<EVENT_TRACE_PROPERTIES *>(&traceBuffer_[0]);
  traceProps->Wnode.BufferSize = static_cast<ULONG>(traceBuffer_.size());
  CoCreateGuid(&traceProps->Wnode.Guid);
  traceProps->Wnode.ClientContext = 2;
  traceProps->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
  traceProps->FlushTimer = 1;
  traceProps->LogFileMode = EVENT_TRACE_USE_LOCAL_SEQUENCE | EVENT_TRACE_REAL_TIME_MODE;
  traceProps->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
}

bool LiveTraceEnumerator::Start()
{
  InitSession();
  EVENT_TRACE_LOGFILEW traceFile;
  ZeroMemory(&traceFile, sizeof(traceFile));
  traceFile.LoggerName = const_cast<wchar_t *>(sessionName_.c_str());

  traceFile.EventCallback = Impl::EventCallback<MOF_DATA_LIVE>;
  traceFile.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
  traceFile.LogfileHeader.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
  //traceFile.LogFileName = GetSessionName();
  auto traceProps = reinterpret_cast<EVENT_TRACE_PROPERTIES *>(&traceBuffer_[0]);
  traceHandle_ = (TRACEHANDLE)INVALID_HANDLE_VALUE;
  auto status = StartTraceW(&traceHandle_, sessionName_.c_str(), traceProps);
  if (status == ERROR_ALREADY_EXISTS)
  {
    Stop();
    InitSession();
    status = StartTraceW(&traceHandle_, sessionName_.c_str(), traceProps);
  }
  if (traceHandle_ == (TRACEHANDLE)INVALID_HANDLE_VALUE)
  {
    return false;
  }
  auto traceGuids = impl_->GetTraceGuids();
  for (auto &&tg : traceGuids)
  {
    std::wcout << L"Enabling trace for " << GuidToString(tg) << std::endl;
    EnableTrace(TRUE, 0xFFFFFFFF, TRACE_LEVEL_INFORMATION, &tg, traceHandle_);
    //EnableTraceEx2(traceHandle_, &tg, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION, 0, 0, 0, );
  }
  FILETIME now;
  GetSystemTimeAsFileTime(&now);
  impl_->SetStartTime(now);
  TRACEHANDLE openTraceHandle = OpenTraceW(&traceFile);
  processor_ = std::make_unique<std::thread>([this, &openTraceHandle]()
  {
    g_thread_enum = impl_.get();
    ProcessTrace(&openTraceHandle, 1, NULL, NULL);
  });
  return true;
}

void LiveTraceEnumerator::Stop()
{
  auto traceProps = reinterpret_cast<EVENT_TRACE_PROPERTIES *>(&traceBuffer_[0]);
  ControlTraceW(traceHandle_, GetSessionName(), traceProps, EVENT_TRACE_CONTROL_STOP);
  if (processor_ && processor_->joinable())
  {
    processor_->join();
    processor_.reset();
  }
}

const wchar_t *LiveTraceEnumerator::GetSessionName() const
{
  return sessionName_.c_str();
}

} // namespace etl


