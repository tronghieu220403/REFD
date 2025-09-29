#include "ole.h"
#include "../ulti/support.h"
#include <objidl.h>
#include <ole2.h>

namespace type_iden {

    // =========================================================
    // Internal helper: MemoryLockBytes
    // =========================================================
    class MemoryLockBytes : public ILockBytes {
    public:
        MemoryLockBytes(const UCHAR* data, size_t size)
            : ref_count_(1), buffer_(data), buffer_size_(size) {
        }
        ~MemoryLockBytes() = default;

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
            void** ppv_object) override {
            if (!ppv_object) return E_POINTER;
            if (riid == IID_IUnknown || riid == IID_ILockBytes) {
                *ppv_object = this;
                AddRef();
                return S_OK;
            }
            *ppv_object = nullptr;
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override {
            return InterlockedIncrement(&ref_count_);
        }

        ULONG STDMETHODCALLTYPE Release() override {
            ULONG res = InterlockedDecrement(&ref_count_);
            if (res == 0) delete this;
            return res;
        }

        HRESULT STDMETHODCALLTYPE ReadAt(ULARGE_INTEGER ul_offset, void* pv, ULONG cb,
            ULONG* pcb_read) override {
            if (ul_offset.QuadPart >= buffer_size_) {
                if (pcb_read) *pcb_read = 0;
                return STG_E_READFAULT;
            }
            ULONG to_read = cb;
            if (ul_offset.QuadPart + cb > buffer_size_) {
                to_read = static_cast<ULONG>(buffer_size_ - ul_offset.QuadPart);
            }
            memcpy(pv, buffer_ + static_cast<size_t>(ul_offset.QuadPart), to_read);
            if (pcb_read) *pcb_read = to_read;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE WriteAt(ULARGE_INTEGER, const void*, ULONG,
            ULONG*) override {
            return STG_E_ACCESSDENIED;
        }

        HRESULT STDMETHODCALLTYPE Flush() override { return S_OK; }
        HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) override {
            return STG_E_ACCESSDENIED;
        }
        HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER,
            DWORD) override {
            return STG_E_INVALIDFUNCTION;
        }
        HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER,
            DWORD) override {
            return STG_E_INVALIDFUNCTION;
        }
        HRESULT STDMETHODCALLTYPE Stat(STATSTG* pstatstg,
            DWORD) override {
            if (!pstatstg) return STG_E_INVALIDPOINTER;
            ZeroMemory(pstatstg, sizeof(STATSTG));
            pstatstg->cbSize.QuadPart = buffer_size_;
            return S_OK;
        }

    private:
        LONG ref_count_;
        const UCHAR* buffer_;
        size_t buffer_size_;
    };

    inline bool ReadEntireStream(IStream* stream, const std::string& path_utf8)
    {
        if (!stream) return false;
        STATSTG st{};
        HRESULT hr = stream->Stat(&st, STATFLAG_NONAME);
        if (FAILED(hr)) {
            cerr << ("Stat failed for stream: " + path_utf8) << endl;
            return false;
        }
        const ULONGLONG declared_size = st.cbSize.QuadPart;

        constexpr ULONG kChunk = 64 * 1024;
        std::unique_ptr<BYTE[]> buf(new BYTE[kChunk]);

        ULONGLONG total_read = 0;
        for (;;) {
            ULONG cb_read = 0;
            hr = stream->Read(buf.get(), kChunk, &cb_read);
            if (FAILED(hr)) {
                cerr << ("Read failed for stream: " + path_utf8) << endl;
                return false;
            }
            if (cb_read == 0) break;
            total_read += cb_read;
            if (total_read > (1ULL << 34)) {  // 16 GB guard
                cerr << ("Stream too large/loop? " + path_utf8) << endl;
                return false;
            }
        }

        if (declared_size != 0 && total_read != declared_size) {
            cerr << "Stream size mismatch at " + path_utf8 + " (declared=" +  ", read=" + std::to_string(total_read) + ")" << endl;
            return false;
        }
        return true;
    }

    inline bool WalkStorageRecursive(IStorage* storage, const std::wstring& path, size_t depth) {
        if (!storage) return false;
        if (depth > 64) {
            cerr << ("Storage nesting too deep.") << endl;
            return false;
        }

        IEnumSTATSTG* enum_stat = nullptr;
        HRESULT hr = storage->EnumElements(0, nullptr, 0, &enum_stat);
        if (FAILED(hr) || !enum_stat) {
            cerr << "EnumElements failed at " + ulti::WstrToStr(path) << endl;
            return false;
        }

        bool ok = true;
        STATSTG st{};
        while (enum_stat->Next(1, &st, nullptr) == S_OK) {
            std::wstring child_name = st.pwcsName ? st.pwcsName : L"";
            std::wstring child_path = path + L"/" + child_name;
            std::string child_path_u8 = ulti::WstrToStr(child_path);

            if (st.type == STGTY_STORAGE) {
                IStorage* child_storage = nullptr;
                hr = storage->OpenStorage(st.pwcsName, nullptr,
                    STGM_READ | STGM_SHARE_EXCLUSIVE,
                    nullptr, 0, &child_storage);
                if (FAILED(hr) || !child_storage) {
                    ok = false;
                    cerr << ("OpenStorage failed at " + child_path_u8) << endl;
                }
                else {
                    if (!WalkStorageRecursive(child_storage, child_path, depth + 1)) {
                        ok = false;
                    }
                    child_storage->Release();
                }
            }
            else if (st.type == STGTY_STREAM) {
                IStream* stream = nullptr;
                hr = storage->OpenStream(st.pwcsName, nullptr,
                    STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stream);
                if (FAILED(hr) || !stream) {
                    ok = false;
                    cerr << ("OpenStream failed at " + child_path_u8);
                }
                else {
                    if (!ReadEntireStream(stream, child_path_u8)) {
                        ok = false;
                    }
                    stream->Release();
                }
            }
            if (st.pwcsName) CoTaskMemFree(st.pwcsName);
        }
        enum_stat->Release();
        return ok;
    }

    // =========================================================
    // Public API
    // =========================================================

    bool DeepValidateOle(const std::span<UCHAR>& data) {
        if (data.size() < 512) {
            cerr << ("Too small for OLE header.");
            return false;
        }

        static const BYTE kOleSignature[8] =
        { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
        if (memcmp(data.data(), kOleSignature, 8) != 0) {
            cerr << ("Missing OLE signature.");
            return false;
        }

        bool coinit_ok = false;
        HRESULT hr_ci = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(hr_ci)) coinit_ok = true;
        defer{ if (coinit_ok) CoUninitialize(); };
        
        MemoryLockBytes* mem_lock_bytes = new MemoryLockBytes(data.data(), data.size());
        if (mem_lock_bytes == nullptr) { return false; }

        IStorage* root = nullptr;
        HRESULT hr = StgOpenStorageOnILockBytes(mem_lock_bytes, nullptr,
            STGM_READ | STGM_SHARE_EXCLUSIVE,
            nullptr, 0, &root);
        mem_lock_bytes->Release();
        if (FAILED(hr) || !root) {
            cerr << ("StgOpenStorageOnILockBytes failed.");
            return false;
        }

        bool ok = WalkStorageRecursive(root, L"__ROOT__", 0);
        root->Release();

        return ok;
    }

    std::vector<std::string> GetOleTypes(const std::span<UCHAR>& data) {
        if (DeepValidateOle(data)) {
            return { "ole" };
        }
        return {};
    }

}  // namespace type_iden
