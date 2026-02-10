#include "etw_controller.h"
#include "ulti/file_helper.h"
#include "receiver.h"

using namespace std;

// ================= Singleton =================
EtwController* EtwController::GetInstance()
{
    static EtwController inst;
    return &inst;
}

// ================= Ctor / Dtor =================
EtwController::EtwController()
    : m_trace(L"hieunt"),
    m_curPid(GetCurrentProcessId())
{
}

EtwController::~EtwController()
{
    Stop();
}

// ================= Debug =================
void EtwController::PrintAllProp(krabs::schema schema, krabs::parser& parser)
{
    std::wstringstream ss;
    ss << L"event_id=" << schema.event_id() << L", ";
    ss << L"task_name=" << schema.task_name() << L", ";
    ss << L"PID=" << schema.process_id() << L", ";

    for (const auto& x : parser.properties())
    {
        std::wstring wstr;
        try { wstr = parser.parse<std::wstring>(x.name()); }
        catch (...) {
            try { wstr = to_wstring(parser.parse<UINT64>(x.name())); }
            catch (...) {
                try { wstr = to_wstring(parser.parse<UINT32>(x.name())); }
                catch (...) {
                    try { wstr = to_wstring(parser.parse<UINT16>(x.name())); }
                    catch (...) {
                        try { wstr = to_wstring(parser.parse<UINT8>(x.name())); }
                        catch (...) {}
                    }
                }
            }
        }

        if (!wstr.empty())
            ss << x.name() << L" " << wstr << L", ";
    }

    if (ss.str().find(L"load\\test") != std::wstring::npos)
        wcout << ss.str() << endl;
}

// ================= Logger =================
void EtwController::PushLog(const std::wstring& s)
{
    if (m_stopLogger == true) {
        return;
    }
    std::lock_guard<std::mutex> l(m_logMutex);
    m_logQueue.push(s);
}

void EtwController::StartLogger()
{
    while (true)
    {
        Sleep(5000);

        if (m_stopLogger) break;

        std::queue<std::wstring> local;

        {
            std::lock_guard<std::mutex> l(m_logMutex);
            if (m_logQueue.empty())
                continue;

            if (m_logQueue.size() >= 10000)
                m_logQueue = std::queue<std::wstring>();

            local.swap(m_logQueue);
        }

        std::wofstream ofs(L"C:\\hieunt_log.jsonl", std::ios::app);
        if (!ofs.is_open())
            continue;

        while (!local.empty())
        {
            ofs << local.front() << L"\n";
            local.pop();
        }
    }
}

// ================= Event worker queue =================
void EtwController::EnqueueEvent(EventInfo&& e)
{
    {
        std::lock_guard<std::mutex> lk(m_evtMutex);

        // Drop oldest if too large (avoid RAM blow)
        if (m_evtQueue.size() >= MAX_EVT_QUEUE) {
            // keep queue moving; drop a chunk
            size_t drop = MAX_EVT_QUEUE / 10;
            while (drop-- && !m_evtQueue.empty()) m_evtQueue.pop_front();
        }

        m_evtQueue.emplace_back(std::move(e));
    }
    m_evtCv.notify_one();
}

void EtwController::EventLoop()
{
    while (true)
    {
        EventInfo e;

        {
            std::unique_lock<std::mutex> lk(m_evtMutex);
            m_evtCv.wait(lk, [&] {
                return m_stopEvt || !m_evtQueue.empty();
                });

            if (m_stopEvt && m_evtQueue.empty())
                return;

            e = std::move(m_evtQueue.front());
            m_evtQueue.pop_front();
        }

        try {
            DispatchEvent(e);
        }
        catch (...) {
            // swallow - keep worker alive
        }
    }
}

void EtwController::DispatchEvent(const EventInfo& e)
{
    if (e.prov == 1)
    {
        // Process provider
        if (e.eid == 1 || e.eid == 15) {
            MaybePrintProcessInfo(0, e.ts, e.pid, e.path);
        }
        return;
    }

    if (e.prov != 2) return;

    switch (e.eid)
    {
    case KFE_CREATE:
    case KFE_CREATE_NEW_FILE:
        HandleFileCreate(e);
        break;

    case KFE_CLEANUP:
    case KFE_CLOSE:
        HandleFileCleanup(e);
        break;

    case KFE_WRITE:
        HandleFileWrite(e);
        break;

    case KFE_RENAME_29:
    case KFE_RENAME:
    case KFE_RENAME_PATH:
        HandleFileRename(e);
        break;

    case KFE_SET_DELETE:
    case KFE_DELETE_PATH:
        HandleFileDelete(e);
        break;

    default:
        break;
    }
}

// ================= IH Cache =================
// Add or update an IH cache entry.
// - Increase ref_count if the entry already exists
// - Update last_used timestamp
// - Perform eviction if the cache is full
void EtwController::IHCacheAdd(ULONGLONG ts, const std::wstring& path, ULONGLONG name_hash)
{
    auto& cache = m_ihCache;

    // Fast path: entry already exists
    auto it = cache.find(name_hash);
    if (it != cache.end()) {
        it->second.ref_count++;
        it->second.last_used_ts = ts;
        return;
    }

    // Evict if cache reaches capacity
    if (cache.size() >= MAX_CACHE_SIZE) {
        auto victim = cache.end();

        // First pass: evict the least-recently-used entry with ref_count == 0
        for (auto it2 = cache.begin(); it2 != cache.end(); ++it2) {
            if (it2->second.ref_count == 0) {
                if (victim == cache.end() ||
                    it2->second.last_used_ts < victim->second.last_used_ts) {
                    victim = it2;
                }
            }
        }

        // Second pass: if no zero-ref entry exists, evict pure LRU
        if (victim == cache.end()) {
            for (auto it2 = cache.begin(); it2 != cache.end(); ++it2) {
                if (victim == cache.end() ||
                    it2->second.last_used_ts < victim->second.last_used_ts) {
                    victim = it2;
                }
            }
        }

        if (victim != cache.end())
            cache.erase(victim);
    }

    // Insert new cache entry
    cache.emplace(
        name_hash,
        IHEntry{
            path,
            1,          // initial reference
            ts
        }
    );
}

// Decrease reference count of an IH cache entry.
// If the entry does not exist, silently ignore.
void EtwController::IHCacheRelease(ULONGLONG name_hash)
{
    auto it = m_ihCache.find(name_hash);
    if (it == m_ihCache.end())
        return;

    if (it->second.ref_count > 0)
        it->second.ref_count--;
}

// Cache identity information (IH) without printing it.
// This function only:
//  - Computes name_hash
//  - Adds the entry to IHCache
// Logging is deferred until a real file operation occurs.
bool EtwController::AddToIHCache(ULONGLONG ts, const std::wstring& path, ULONGLONG* out_name_hash)
{
    if (path.empty())
        return false;

    std::wstring lower = ulti::ToLower(path);
    ULONGLONG name_hash = helper::GetWstrHash(lower);
    if (out_name_hash != nullptr) {
        *out_name_hash = name_hash;
    }

    std::lock_guard<std::mutex> lock(m_identityMutex);

    // If this IH has already been printed, skip completely
    if (m_printedNameHash.contains(name_hash))
        return false;

    // Otherwise, cache it and increase reference count
    IHCacheAdd(ts, lower, name_hash);
    return true;
}

// Materialize (print) an IH entry.
// This should be called right before a guaranteed file operation log.
// Once printed, the entry is:
//  - Inserted into printedNameHash LRU
//  - Removed from IHCache regardless of ref_count
void EtwController::MaybePrintIH(ULONGLONG name_hash, std::wstring* p_out_name)
{
    std::lock_guard<std::mutex> lock(m_identityMutex);
    // Already printed -> nothing to do
    if (m_printedNameHash.contains(name_hash) == true) {
        if (p_out_name != nullptr) {
            m_printedNameHash.get(name_hash, *p_out_name);
        }
        return;
    }

    auto it = m_ihCache.find(name_hash);
    if (it == m_ihCache.end()) {
        return;
    }

    const auto& entry = it->second;

    if (p_out_name != nullptr) {
        *p_out_name = entry.path;
    }

    std::wstringstream ss;
    ss << L"F,IH,0,0," << name_hash << L"," << entry.path;
    PushLog(ss.str());

    // Update printed IH LRU cache
    m_printedNameHash.put(name_hash, std::move(entry.path));

    // Remove from IHCache after materialization
    m_ihCache.erase(it);
}

// ================= Process logging =================
void EtwController::MaybePrintProcessInfo(ULONG eid, ULONGLONG ts, ULONG pid, const std::wstring& path)
{
    if (path.empty())
        return;

    std::wstringstream ss;
    ss << L"P,I," << eid << L"," << ts << L"," << pid << L"," << path;
    PushLog(ss.str());
}

// ================= Identity logging =================
void EtwController::MaybePrintIO(ULONGLONG file_object, ULONGLONG name_hash)
{
    if (file_object == 0 || name_hash == 0)
        return;

    std::lock_guard<std::mutex> lock(m_identityMutex);
    if (m_printedObj.contains(file_object) == true) {
        return;
    }

    std::wstringstream ss;
    ss << L"F,IO,0," << file_object << L"," << name_hash;
    PushLog(ss.str());

    m_printedObj.insert(file_object);
}

//void EtwController::ForcePrintIK(ULONG eid, ULONGLONG file_key, ULONGLONG name_hash)
//{
//    if (file_key == 0 || name_hash == 0)
//        return;
//
//    std::wstringstream ss;
//    ss << L"F,IK," << eid << L"," << file_key << L"," << name_hash;
//    PushLog(ss.str());
//}

// ================= File operation logging =================
void EtwController::LogFileCreateOperation(ULONG pid, ULONG eid, ULONGLONG ts, ULONGLONG name_hash)
{
    std::wstringstream ss;
    ss << L"F,C," << eid << L"," << pid << L"," << ts << L"," << name_hash;
    PushLog(ss.str());
}

void EtwController::LogFileWriteOperation(ULONG pid, ULONG eid, ULONGLONG ts, ULONGLONG file_object)
{
    std::wstringstream ss;
    ss << L"F,W," << eid << L"," << pid << L"," << ts << L"," << file_object;
    PushLog(ss.str());
}

void EtwController::LogFileRenameOperation(ULONG pid, ULONG eid, ULONGLONG ts, ULONGLONG name_hash, ULONGLONG file_object, ULONGLONG file_key)
{
    std::wstringstream ss;
    ss << L"F,RN," << eid << L"," << pid << L"," << ts << L"," << name_hash << L"," << file_object << L"," << file_key;
    PushLog(ss.str());
}

void EtwController::LogFileDeleteOperation(ULONG pid, ULONG eid, ULONGLONG ts, ULONGLONG name_hash, ULONGLONG file_object, ULONGLONG file_key)
{
    std::wstringstream ss;
    ss << L"F,D," << eid << L"," << pid << L"," << ts << L"," << name_hash << L"," << file_object << L"," << file_key;
    PushLog(ss.str());
}

// ================= File handlers =================
void EtwController::HandleFileCreate(const EventInfo& e)
{
    ULONGLONG fo = e.file_object;
    std::wstring path = e.path;
    UINT32 co = e.has_create_options ? e.create_options : 0;
    ULONG eid = e.eid;
    ULONG pid = e.pid;
    ULONGLONG ts = e.ts;

    // Parse done -> identity decisions first
    ULONGLONG name_hash = 0;
    AddToIHCache(ts, path, &name_hash);

    // Manage object table for later lookups
    {
        std::lock_guard<std::mutex> lock(m_identityMutex);
        m_objToNameHash[fo] = name_hash;
        m_printedWriteObj.erase(fo);
    }

    // Operation
    if (eid == KFE_CREATE_NEW_FILE)
    {
        MaybePrintIH(name_hash, nullptr);
        MaybePrintIO(fo, name_hash);
        
        manager::Receiver::GetInstance()->PushFileEvent(path, pid);
        LogFileCreateOperation(pid, eid, ts, name_hash);
    }

    // Delete-on-close mapped to delete operation without file key
    if ((co & 0x00001000) != 0)
    {
        MaybePrintIH(name_hash, nullptr);
        MaybePrintIO(fo, name_hash);
        LogFileDeleteOperation(pid, eid, ts, name_hash, fo, 0);
    }
}

void EtwController::HandleFileCleanup(const EventInfo& e)
{
    ULONGLONG fo = e.file_object;
    {
        std::lock_guard<std::mutex> lock(m_identityMutex);
        m_objToNameHash.erase(fo);
        m_printedObj.erase(fo);
        m_printedWriteObj.erase(fo);
    }
}

void EtwController::HandleFileWrite(const EventInfo& e)
{
    ULONGLONG fo = e.file_object;
    ULONG eid = e.eid;
    ULONG pid = e.pid;
    ULONGLONG ts = e.ts;

    m_identityMutex.lock();
    if (m_printedWriteObj.contains(fo) == true) {
        m_identityMutex.unlock();
        return;
    }
    m_identityMutex.unlock();

    // Parse done -> identity decisions first (IO requires name_hash, resolve via obj table)
    ULONGLONG name_hash = 0;
    {
        std::lock_guard<std::mutex> lock(m_identityMutex);
        auto it = m_objToNameHash.find(fo);
        if (it != m_objToNameHash.end())
            name_hash = it->second;
    }

    std::wstring path;

    MaybePrintIH(name_hash, &path);
    MaybePrintIO(fo, name_hash);

    manager::Receiver::GetInstance()->PushFileEvent(path, pid);

    // Operation
    LogFileWriteOperation(pid, eid, ts, fo);
    m_identityMutex.lock();
    m_printedWriteObj.insert(fo);
    m_identityMutex.unlock();
}

void EtwController::HandleFileRename(const EventInfo& e) // File name in 19 rename to file name in event 27
{
    ULONGLONG fo = 0;
    ULONGLONG key = 0;
    std::wstring path;
    ULONGLONG name_hash = 0;
    ULONG eid = e.eid;
    ULONGLONG ts = e.ts;
    ULONG pid = e.pid;

    if (eid == KFE_RENAME_PATH)
    {
        path = e.path;
        AddToIHCache(ts, path, &name_hash);
        std::wstring path;
        MaybePrintIH(name_hash, &path);
        manager::Receiver::GetInstance()->PushFileEvent(path, pid);
    }
    else
    {
        fo = e.file_object;
        {
            std::lock_guard<std::mutex> lock(m_identityMutex);
            auto it = m_objToNameHash.find(fo);
            if (it != m_objToNameHash.end())
                name_hash = it->second;
        }
        MaybePrintIH(name_hash, nullptr);
    }

    key = e.file_key;

    // Parse done -> identity decisions first
    if (fo != 0)
        MaybePrintIO(fo, name_hash);

    // Operation
    LogFileRenameOperation(pid, eid, ts, name_hash, fo, key);
}

void EtwController::HandleFileDelete(const EventInfo& e)
{
    ULONGLONG fo = 0;
    ULONGLONG key = 0;
    std::wstring path;
    ULONGLONG name_hash = 0;
    ULONG eid = e.eid;
    ULONGLONG ts = e.ts;
    ULONG pid = e.pid;

    switch (eid) {
    case KFE_NAME_DELETE:
        //key = e.file_key;
        //path = e.path;

        //AddToIHCache(ts, path, name_hash);
        //MaybePrintIH(name_hash);
        //ForcePrintIK(eid, key, name_hash);

        break;

    case KFE_DELETE_PATH:
        path = e.path;
        AddToIHCache(ts, path, &name_hash);
        MaybePrintIH(name_hash, nullptr);
        key = e.file_key;

        // If event provides FileObject, print IO too
        fo = e.file_object;
        if (fo != 0)
            MaybePrintIO(fo, name_hash);
        LogFileDeleteOperation(pid, eid, ts, name_hash, fo, key);

        break;

    case KFE_SET_DELETE:
        fo = e.file_object;
        key = e.file_key;
        {
            std::lock_guard<std::mutex> lock(m_identityMutex);
            auto it = m_objToNameHash.find(fo);
            if (it != m_objToNameHash.end())
                name_hash = it->second;
        }
        MaybePrintIH(name_hash, nullptr);

        // Parse done -> identity decisions first
        MaybePrintIO(fo, name_hash);

        // Operation
        LogFileDeleteOperation(pid, eid, ts, name_hash, fo, key);

        break;

    default:
        break;
    }
}

// ================= ETW =================
void EtwController::StartProviderBlocking()
{
    // ===== Process provider =====
    krabs::provider<> proc(L"Microsoft-Windows-Kernel-Process");
    proc.any(0x10);
    proc.enable_rundown_events();

    auto proc_cb = [this](const EVENT_RECORD& r, const krabs::trace_context& c)
        {
            auto eid = r.EventHeader.EventDescriptor.Id;
            if (eid != 1 && eid != 2 && eid != 15) return;

            krabs::schema s(r, c.schema_locator);
            krabs::parser p(s);

            EventInfo e;
            e.prov = 1;
            e.eid = (ULONG)eid;
            e.ts = s.timestamp().QuadPart;

            try {
                e.pid = p.parse<ULONG>(L"ProcessID");
                e.path = p.parse<std::wstring>(L"ImageName");
            }
            catch (...) {
                return;
            }

            if (eid == 1 || eid == 15) {
                EnqueueEvent(std::move(e));
            }
        };

    krabs::event_filter pf(krabs::predicates::any_event);
    pf.add_on_event_callback(proc_cb);
    proc.add_filter(pf);

    m_trace.enable(proc);

    // ===== File provider =====
    krabs::provider<> file(L"Microsoft-Windows-Kernel-File");
    file.any(0x10 | 0x20 | 0x80 | 0x200 | 0x400 | 0x800 | 0x1000);
    //file.enable_rundown_events();

    auto file_cb = [this](const EVENT_RECORD& r, const krabs::trace_context& c)
        {
            krabs::schema s(r, c.schema_locator);
            krabs::parser parser(s);
            
            //PrintAllProp(s, parser);
            //return;
            
            uint32_t pid = s.process_id();
            if (pid == m_curPid || pid == 4) {
                return;
            }
            
            uint64_t ts = s.timestamp().QuadPart;
            auto eid = s.event_id();
            //PushLog(wstring(L"EID: ") + to_wstring(eid));
            
            EventInfo e;
            e.prov = 2;
            e.eid = eid;
            e.pid = pid;
            e.ts = ts;

            try {
                switch (eid)
                {
                case KFE_CREATE:
                case KFE_CREATE_NEW_FILE: // Create -> save to cache to retrieve file name event, also check for FILE_DELETE_ON_CLOSE (0x00001000) of CreateOptions
                    e.file_object = (ULONGLONG)parser.parse<PVOID>(L"FileObject");
                    e.path = parser.parse<std::wstring>(L"FileName");
                    e.create_options = parser.parse<UINT32>(L"CreateOptions");
                    e.has_create_options = true;
                    EnqueueEvent(std::move(e));
                    break;

                case KFE_CLEANUP:
                case KFE_CLOSE: // Close, clean up -> remove from check
                    e.file_object = (ULONGLONG)parser.parse<PVOID>(L"FileObject");
                    EnqueueEvent(std::move(e));
                    break;

                case KFE_WRITE: // Write
                    e.file_object = (ULONGLONG)parser.parse<PVOID>(L"FileObject");
                    EnqueueEvent(std::move(e));
                    break;

                case KFE_RENAME_29:
                case KFE_RENAME:
                    e.file_object = (ULONGLONG)parser.parse<PVOID>(L"FileObject");
                    parser.try_parse<ULONGLONG>(L"FileKey", e.file_key);
                    EnqueueEvent(std::move(e));
                    break;

                case KFE_RENAME_PATH: // Rename
                    parser.try_parse<std::wstring>(L"FilePath", e.path);
                    parser.try_parse<ULONGLONG>(L"FileKey", e.file_key);
                    EnqueueEvent(std::move(e));
                    break;

                case KFE_NAME_DELETE: //NameDelete
                    //parser.try_parse<std::wstring>(L"FileName", e.path);
                    //parser.try_parse<ULONGLONG>(L"FileKey", e.file_key);
                    //EnqueueEvent(std::move(e));
                    break;

                case KFE_SET_DELETE:
                    e.file_object = (ULONGLONG)parser.parse<PVOID>(L"FileObject");
                    parser.try_parse<ULONGLONG>(L"FileKey", e.file_key);
                    EnqueueEvent(std::move(e));
                    break;

                case KFE_DELETE_PATH: // SetDelete, DeletePath
                    parser.try_parse<std::wstring>(L"FileName", e.path);
                    parser.try_parse<ULONGLONG>(L"FileKey", e.file_key);
                    parser.try_parse<ULONGLONG>(L"FileObject", e.file_object);
                    EnqueueEvent(std::move(e));
                    break;

                default:
                    break;
                }
            }
            catch (...) {}
        };

    krabs::event_filter ff(krabs::predicates::any_event);
    ff.add_on_event_callback(file_cb);
    file.add_filter(ff);

    m_trace.enable(file);

    m_trace.start(); // blocking
}

// ================= Lifecycle =================
void EtwController::Start()
{
    m_loggerThread = std::jthread([this]() {
        StartLogger();
        });

    // Worker thread: consume ETW events
    m_evtThread = std::jthread([this]() {
        EventLoop();
        });

    // https://lowleveldesign.wordpress.com/2020/08/15/fixing-empty-paths-in-fileio-events-etw
    m_traceThread = std::jthread([this]() {
        StartProviderBlocking();
        });
}

void EtwController::Stop()
{
    // Stop trace first -> callbacks stop coming
    m_trace.stop();

    // Stop event worker
    {
        std::lock_guard<std::mutex> lk(m_evtMutex);
        m_stopEvt = true;
    }
    m_evtCv.notify_all();
    if (m_evtThread.joinable())
        m_evtThread.join();

    m_stopLogger = true;
    if (m_loggerThread.joinable())
        m_loggerThread.join();
}

#ifdef _DEBUG
void EtwController::RunDebugBlocking()
{
    StartProviderBlocking();
}
#endif
