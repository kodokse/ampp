#include "stdafx.h"
#define INITGUID
#include <ampp/etl/trace_enumerator.h>
#include <ampp/etl/time_util.h>
#include <ampp/etl/guid_util.h>
#include <ampp/etl/string_util.h>
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
struct MofDataBase
{
  GUID  sourceFileGUID;
  FILETIME timeStamp;
  DWORD threadId;
  DWORD processId;
  BYTE  *params;
};
struct MofDataFromFile : public MofDataBase
{
  FILETIME GetTimeStamp();
};
struct MofDataLive : public MofDataBase
{
  FILETIME GetTimeStamp();
};
#pragma pack(pop)

const wchar_t TIME_FORMAT[] = L"Y-M-D H:m:S.l";

FILETIME MofDataFromFile::GetTimeStamp()
{
  return timeStamp;
}

FILETIME MofDataLive::GetTimeStamp()
{
  static LARGE_INTEGER qpf = { 0 };
  static LARGE_INTEGER reference = { 0 };
  if (qpf.QuadPart == 0)
  {
    QueryPerformanceFrequency(&qpf);
  }
  if (reference.QuadPart == 0)
  {
    memcpy(&reference, &timeStamp, sizeof(reference));
  }
  LARGE_INTEGER li;
  memcpy(&li, &timeStamp, sizeof(li));
  li.QuadPart = ((li.QuadPart - reference.QuadPart) * 10000000LL) / qpf.QuadPart;
  FILETIME rv;
  memcpy(&rv, &li, sizeof(rv));
  return rv;
}

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
    it->second(
      [this](const GUID &fileGuid, const wchar_t *fileName, TraceFormat &&traceFormat)
      {
        if(AddSourceFileTrace(fileGuid, std::move(traceFormat)))
        {
          SetSourceFilePath(fileGuid, fileName);
        }
      });
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
  bool addedProvider = false;
  provEnum.EnumerateTraces(
    [this, &addedProvider](const GUID &fileGuid, const ProviderCallback &prov)
    {
      impl_->AddProvider(fileGuid, prov);
      addedProvider = true;
    },
    [this, &addedProvider](const GUID &traceGuid, const wchar_t *traceName)
    {
      impl_->AddTraceGuid(traceGuid, traceName);
      addedProvider = true;
    });
  if(addedProvider)
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
  void *metadata_;
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
    , metadata_(nullptr)
  {
    values.resize(static_cast<size_t>(TraceEventDataItem::MAX_ITEM));
    (*this)[TraceEventDataItem::TimeStamp] = FormatFileTime(TIME_FORMAT, ts);
    (*this)[TraceEventDataItem::ProcessId] = std::to_wstring(pid);
    (*this)[TraceEventDataItem::ThreadId] = std::to_wstring(tid);
    (*this)[TraceEventDataItem::TraceIndex] = std::to_wstring(order);
  }
  TraceEventItem(const std::vector<std::wstring> &items)
    : traceIdx(0)
    , dataState(DataState::HasData)
    , metadata_(nullptr)
    , values(items)
  {
  }
  TraceEventItem()
    : traceIdx(0)
    , dataState(DataState::HasData)
    , metadata_(nullptr)
    , values(static_cast<size_t>(TraceEventDataItem::MAX_ITEM))
  {
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
    (*this)[TraceEventDataItem::SourceFile] = db.GetSourceFile(fileGuid).filename();
    (*this)[TraceEventDataItem::Message] = FormatTraceFormat(*traceFmt, fmtData);
    (*this).dataState = DataState::HasData;
    return true;
  }
};

class TraceEnumerator::Impl : public Observer
{
  friend class TraceEnumerator;
  using FilterList = std::list<std::function<bool(TraceEventDataItem item, const std::wstring &value)>>;
public:
  TraceEnumerator::Impl::Impl(const FormatDatabase *db);
  TraceEnumerator::Impl::~Impl();
  void SetStartTime(const FILETIME &startTime);
  void SetStartTime(const LARGE_INTEGER &startTime);
  size_t GetItemCount() const;
  const wchar_t *GetItemValue(size_t index, TraceEventDataItem item, size_t *valueLength) const;
  const std::wstring &GetItemValue(size_t index, TraceEventDataItem item) const;
  void InjectItem(const FILETIME &timeStamp, const std::function<std::wstring(TraceEventDataItem item)> &itemValue);
  void *GetItemMetadata(size_t index);
  bool SetItemMetadata(size_t index, void *metadata);
  void ApplyFilters();
  template <class MofType>
  static void CALLBACK EventCallback(PEVENT_TRACE pEvent);
  void InsertItem(std::unique_ptr<TraceEventItem> item);
  std::vector<GUID> GetTraceGuids() const;
  void SetProvidersUpdatedCallback(const std::function<void ()> &pup);
  void ClearAllTraces();
  //
  void Notify(const Observable *o) override;
private:
  bool GenerateLogEntry(const GUID &fileGuid, DWORD traceId, const FILETIME &timeStamp, DWORD processId, DWORD threadId, PVOID params, size_t paramLen);
  bool TestFilters(TraceEventItem &tei);
  bool EvaluateItem(TraceEventItem &tei) const;
  std::pair<FilterList::const_iterator, FilterList::const_iterator> GetFilterRange() const;
  template <class PtrT>
  size_t FindInsertPosition(const std::vector<PtrT> &v, const std::wstring &timeStamp) const;
  template <class PtrT>
  size_t FindInitialInsertPosition(const std::vector<PtrT> &v, const std::wstring &timeStamp) const;
private:
  const FormatDatabase *db_;
  LogCallback logger_;
  CountCallback countCallback_;
  FILETIME startTime_;
  std::vector<std::unique_ptr<TraceEventItem>> allTraces_;
  std::vector<TraceEventItem *> filteredTraceEvents_;
  mutable std::mutex traceLock_;
  FilterList filters_;
  mutable std::mutex filterLock_;
  std::function<void ()> providersUpdated_;
  int uniqueCounter_;
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

void TraceEnumerator::Impl::SetProvidersUpdatedCallback(const std::function<void()>& pup)
{
  providersUpdated_ = pup;
}

void TraceEnumerator::Impl::Notify(const Observable *o)
{
  {
    std::lock_guard<std::mutex> l(traceLock_);
    for (auto &&te : allTraces_)
    {
      if (te->dataState == DataState::NoProvider)
      {
        te->dataState = DataState::NoData;
      }
    }
  }
  if(providersUpdated_)
  {
    providersUpdated_();
  }
}

void TraceEnumerator::Impl::ClearAllTraces()
{
  std::lock_guard<std::mutex> l(traceLock_);
  filteredTraceEvents_.clear();
  allTraces_.clear();
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
  {
    std::lock_guard<std::mutex> l(impl_->filterLock_);
    impl_->filters_.push_back(filter);
  }
  impl_->ApplyFilters();
}

void TraceEnumerator::RemoveAllFilters()
{
  {
    std::lock_guard<std::mutex> l(impl_->filterLock_);
    impl_->filters_.clear();
  }
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

void *TraceEnumerator::GetItemMetadata(size_t index)
{
  return impl_->GetItemMetadata(index);
}

bool TraceEnumerator::SetItemMetadata(size_t index, void *metadata)
{
  return impl_->SetItemMetadata(index, metadata);
}

void TraceEnumerator::InjectItem(const FILETIME &timeStamp, const std::function<std::wstring(TraceEventDataItem item)> &itemValue)
{
  impl_->InjectItem(timeStamp, itemValue);
}

void TraceEnumerator::ApplyFilters()
{
  impl_->ApplyFilters();
}

void TraceEnumerator::RemoveAllItems()
{
  impl_->ClearAllTraces();
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
  auto filterRange = GetFilterRange();
  tei.EvaluateItem(*db_);
  for(auto it = filterRange.first; it != filterRange.second; ++it)
  {
    for(auto v = TraceEventDataItem::TraceIndex; v < TraceEventDataItem::MAX_ITEM; ++v)
    {
      if(!(*it)(v, tei[v]))
      {
        return false;
      }
    }
  }
  return true;
}

void TraceEnumerator::Impl::ApplyFilters()
{
  {
    std::lock_guard<std::mutex> l(traceLock_);
    filteredTraceEvents_.clear();
    for (auto &&te : allTraces_)
    {
      if (TestFilters(*te))
      {
        filteredTraceEvents_.push_back(te.get());
      }
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
  DWORD offset = 0;
  if (pEvent->Header.Class.Version >= 0x80)
  {
    offset += sizeof(DWORD);
  }
  auto mofData = reinterpret_cast<MofType *>(reinterpret_cast<char *>(pEvent->MofData) + offset);
  DWORD traceId = LOWORD(pEvent->Header.Version);
  auto tei = std::make_unique<TraceEventItem>(mofData->sourceFileGUID, context->startTime_ + mofData->GetTimeStamp(), traceId, mofData->processId, mofData->threadId, static_cast<DWORD>(context->allTraces_.size()), &mofData->params, pEvent->MofLength);
  context->EvaluateItem(*tei);
  context->InsertItem(std::move(tei));
  if (context->logger_)
  {
    context->GenerateLogEntry(mofData->sourceFileGUID, traceId, context->startTime_ + mofData->timeStamp, mofData->processId, mofData->threadId, &mofData->params, pEvent->MofLength);
  }
}

template <class U, class T>
void InsertOrAppend(std::vector<U> &v, size_t index, T &&value)
{
  if (index >= v.size())
  {
    v.push_back(std::forward<T>(value));
  }
  else
  {
    v.insert(v.begin() + index, std::forward<T>(value));
  }
}

void TraceEnumerator::Impl::InsertItem(std::unique_ptr<TraceEventItem> item)
{
  size_t filteredItemCount = 0;
  const bool filterMatch = TestFilters(*item);
  {
    std::lock_guard<std::mutex> l(traceLock_);
    //
    auto timeStamp = (*item)[TraceEventDataItem::TimeStamp];
    auto aindex = FindInsertPosition(allTraces_, timeStamp);
    (*item)[TraceEventDataItem::TraceIndex] = std::to_wstring(uniqueCounter_++);
    auto rawPtr = item.get(); // gets lost once we move
    InsertOrAppend(allTraces_, aindex, std::move(item));
    if (filterMatch)
    {
      auto findex = FindInsertPosition(filteredTraceEvents_, timeStamp);
      InsertOrAppend(filteredTraceEvents_, findex, rawPtr);
    }
    filteredItemCount = filteredTraceEvents_.size();
  }
  if (countCallback_)
  {
    countCallback_(filteredItemCount);
  }
}

bool TraceEnumerator::Impl::EvaluateItem(TraceEventItem &ev) const
{
  return ev.EvaluateItem(*db_);
}

std::pair<TraceEnumerator::Impl::FilterList::const_iterator, TraceEnumerator::Impl::FilterList::const_iterator> TraceEnumerator::Impl::GetFilterRange() const
{
  std::lock_guard<std::mutex> l(filterLock_);
  return std::make_pair(filters_.cbegin(), filters_.cend());
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
  std::lock_guard<std::mutex> l(traceLock_);
  return filteredTraceEvents_.size();
}

const std::wstring &TraceEnumerator::Impl::GetItemValue(size_t index, TraceEventDataItem item) const
{
  static std::wstring EmptyString;
  TraceEventItem *ev = nullptr;
  {
    std::lock_guard<std::mutex> l(traceLock_);
    if (index >= filteredTraceEvents_.size())
    {
      return EmptyString;
    }
    ev = filteredTraceEvents_[index];
  }
  EvaluateItem(*ev);
  return (*ev)[item];
}

const wchar_t *TraceEnumerator::Impl::GetItemValue(size_t index, TraceEventDataItem item, size_t *valueLength) const
{
  TraceEventItem *ev = nullptr;
  {
    std::lock_guard<std::mutex> l(traceLock_);
    if (index >= filteredTraceEvents_.size())
    {
      return nullptr;
    }
    ev = filteredTraceEvents_[index];
  }
  EvaluateItem(*ev);
  if(valueLength)
  {
    *valueLength = (*ev)[item].length();
  }
  return (*ev)[item].c_str();
}

void *TraceEnumerator::Impl::GetItemMetadata(size_t index)
{
  std::lock_guard<std::mutex> l(traceLock_);
  if (index >= filteredTraceEvents_.size())
  {
    return nullptr;
  }
  return filteredTraceEvents_[index]->metadata_;
}

bool TraceEnumerator::Impl::SetItemMetadata(size_t index, void *metadata)
{
  std::lock_guard<std::mutex> l(traceLock_);
  if (index >= filteredTraceEvents_.size())
  {
    return false;
  }
  filteredTraceEvents_[index]->metadata_ = metadata;
  return true;
}

template <class PtrT>
size_t TraceEnumerator::Impl::FindInsertPosition(const std::vector<PtrT> &v, const std::wstring &timeStamp) const
{
  auto cur = FindInitialInsertPosition(v, timeStamp);
  // 1,2,4,5
  // 1,2,3,3,3,3,3,4,5,6
  // we want the *last* position where timestamps are equal
  while (cur < v.size())
  {
    if (timeStamp != (*v[cur])[TraceEventDataItem::TimeStamp])
    {
      break;
    }
    cur++;
  }
  return cur;
}

template <class PtrT>
size_t TraceEnumerator::Impl::FindInitialInsertPosition(const std::vector<PtrT> &v, const std::wstring &timeStamp) const
{
  size_t end = v.size() - 1;
  size_t start = 0;
  size_t index = std::numeric_limits<size_t>::max();
  while (end < v.size() && end >= start)
  {
    auto cur = (start + end) / 2;
    auto cmp = wcscmp(timeStamp.c_str(), (*v[cur])[TraceEventDataItem::TimeStamp].c_str());
    if (cmp > 0)
    {
      start = cur + 1;
    }
    else if (cmp < 0)
    {
      end = cur - 1;
    }
    else
    {
      return cur;
    }
  }
  return start;
}

void TraceEnumerator::Impl::InjectItem(const FILETIME &timeStamp, const std::function<std::wstring(TraceEventDataItem item)> &itemValue)
{
  auto tei = std::make_unique<TraceEventItem>();
  for (TraceEventDataItem item = TraceEventDataItem::ModuleName; item < TraceEventDataItem::MAX_ITEM; ++item)
  {
    if (item == TraceEventDataItem::TimeStamp)
    {
      continue;
    }
    (*tei)[item] = itemValue(item);
  }
  (*tei)[TraceEventDataItem::TimeStamp] = FormatFileTime(TIME_FORMAT, timeStamp);
  //
  InsertItem(std::move(tei));
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
  traceFile.EventCallback = Impl::EventCallback<MofDataFromFile>;
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
  , traceHandle_((TRACEHANDLE)INVALID_HANDLE_VALUE)
{
}

void LiveTraceEnumerator::InitSession()
{
  traceBuffer_.resize((sessionName_.length() + 1) * sizeof(wchar_t) + sizeof(EVENT_TRACE_PROPERTIES));
  auto traceProps = reinterpret_cast<EVENT_TRACE_PROPERTIES *>(&traceBuffer_[0]);
  traceProps->Wnode.BufferSize = static_cast<ULONG>(traceBuffer_.size());
  CoCreateGuid(&traceProps->Wnode.Guid);
  traceProps->Wnode.ClientContext = 1;
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

  traceFile.EventCallback = Impl::EventCallback<MofDataLive>;
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
    EnableTrace(TRUE, 0xFFFFFFFF, TRACE_LEVEL_INFORMATION, &tg, traceHandle_);
    //EnableTraceEx2(traceHandle_, &tg, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION, 0, 0, 0, );
  }
  impl_->SetStartTime(GetCurrentLocalFileTime());
  TRACEHANDLE openTraceHandle = OpenTraceW(&traceFile);
  processor_ = std::make_unique<std::thread>([this, &openTraceHandle]()
  {
    g_thread_enum = impl_.get();
    ProcessTrace(&openTraceHandle, 1, NULL, NULL);
  });
  impl_->SetProvidersUpdatedCallback([this]()
  {
    auto traceGuids = impl_->GetTraceGuids();
    for (auto &&tg : traceGuids)
    {
      EnableTrace(TRUE, 0xFFFFFFFF, TRACE_LEVEL_INFORMATION, &tg, traceHandle_);
    }
  });
  return true;
}

void LiveTraceEnumerator::Stop()
{
  if (traceHandle_ == (TRACEHANDLE)INVALID_HANDLE_VALUE)
  {
    return;
  }
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

/////////////////////////////////////

template <class CharT>
std::vector<CharT> ReadWholeFile(const fs::path &filePath)
{
  std::vector<CharT> data;
  auto h = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE)
  {
    return data;
  }
  auto fileSize = GetFileSize(h, nullptr);
  if (fileSize == 0)
  {
    CloseHandle(h);
    return data;
  }
  data.resize((fileSize + (sizeof(CharT) - 1)) / sizeof(CharT));
  if (!ReadFile(h, &data[0], fileSize, &fileSize, nullptr))
  {
    CloseHandle(h);
    data.clear();
    return data;
  }
  CloseHandle(h);
  return data;
}

TxtfileEnumerator::TxtfileEnumerator(const FormatDatabase &db, const fs::path &logPath)
  : TraceEnumerator(db)
  , logPath_(logPath)
{
}

bool TxtfileEnumerator::Start()
{
  std::map<size_t, size_t> eventItemMapper {
    {0, (size_t)TraceEventDataItem::TraceIndex},
    {1, (size_t)TraceEventDataItem::ModuleName},
    {2, (size_t)TraceEventDataItem::ProcessId},
    {3, (size_t)TraceEventDataItem::ThreadId},
    {4, (size_t)TraceEventDataItem::SourceFile},
    {5, (size_t)TraceEventDataItem::Function},
    {6, (size_t)TraceEventDataItem::TimeStamp},
    {7, (size_t)TraceEventDataItem::Message},
    {8, (size_t)TraceEventDataItem::LineNumber}
  };
  const std::map<std::string, size_t> eventStringItemMapper {
    { "ID", (size_t)TraceEventDataItem::TraceIndex },
    { "LOG", (size_t)TraceEventDataItem::ModuleName },
    { "PROCESS", (size_t)TraceEventDataItem::ProcessId },
    { "THREAD", (size_t)TraceEventDataItem::ThreadId },
    { "FILE", (size_t)TraceEventDataItem::SourceFile },
    { "FUNCTION", (size_t)TraceEventDataItem::Function },
    { "TIMESTAMP", (size_t)TraceEventDataItem::TimeStamp },
    { "MESSAGE", (size_t)TraceEventDataItem::Message },
    { "LINE", (size_t)TraceEventDataItem::LineNumber }
  };
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  auto fileData = ReadWholeFile<char>(logPath_);
  if (fileData.empty())
  {
    return false;
  }
  const char *it = &fileData[0];
  const char *end = it + fileData.size();
  bool firstLine = true;
  auto lines = Split<char>(it, end, '\n');
  for (auto &&l : lines)
  {
    TrimR(l);
    auto items = Split<char>(l, '\t');
    if (firstLine)
    {
      firstLine = false;
      for (size_t n = 0; n < items.size(); ++n)
      {
        auto it = eventStringItemMapper.find(items[n]);
        if (it == eventStringItemMapper.end())
        {
          continue;
        }
        firstLine = true;
        eventItemMapper[n] = it->second;
      }
    }
    //// if we have a header, don't create a log entry for it
    if (firstLine)
    {
      firstLine = false;
      continue;
    }
    std::vector<std::wstring> witems((size_t)TraceEventDataItem::MAX_ITEM);
    size_t i = 0;
    for (auto &&im : items)
    {
      if (i >= witems.size())
      {
        break;
      }
      witems[eventItemMapper.at(i++)] = converter.from_bytes(im);
    }
    auto tei = std::make_unique<TraceEventItem>(witems);
    impl_->InsertItem(std::move(tei));
  }
  return true;
}

void TxtfileEnumerator::Stop()
{
}


} // namespace etl


