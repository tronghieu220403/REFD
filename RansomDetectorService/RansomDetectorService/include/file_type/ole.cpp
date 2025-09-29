#include "ole.h"
#include <objidl.h>

namespace type_iden {

    // MemoryLockBytes implements ILockBytes for wrapping memory buffer
    class MemoryLockBytes : public ILockBytes {
    public:
        MemoryLockBytes(const UCHAR* data, size_t size)
            : ref_count_(1), buffer_(data), buffer_size_(size) {
        }

        // IUnknown
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

        // ILockBytes
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
            memcpy(pv, buffer_ + ul_offset.QuadPart, to_read);
            if (pcb_read) *pcb_read = to_read;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE WriteAt(ULARGE_INTEGER, const void*, ULONG,
            ULONG*) override {
            return STG_E_ACCESSDENIED;  // read-only
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
            DWORD grf_stat_flag) override {
            if (!pstatstg) return STG_E_INVALIDPOINTER;
            ZeroMemory(pstatstg, sizeof(STATSTG));
            pstatstg->cbSize.QuadPart = buffer_size_;
            return S_OK;
        }

    private:
        ~MemoryLockBytes() = default;

        LONG ref_count_;
        const UCHAR* buffer_;
        size_t buffer_size_;
    };

    // Returns {"ole"} if file is valid OLE Compound Document, else empty vector.
    vector<string> GetOleTypes(const span<UCHAR>& data) {
        if (data.size() < 512) {
            return {};
        }

        // OLE signature (D0 CF 11 E0 A1 B1 1A E1)
        static const BYTE kOleSignature[8] =
        { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
        if (memcmp(data.data(), kOleSignature, 8) != 0) {
            return {};
        }

        MemoryLockBytes* mem_lock_bytes =
            new MemoryLockBytes(data.data(), data.size());

        // Check via StgIsStorageILockBytes
        HRESULT hr = StgIsStorageILockBytes(mem_lock_bytes);
        if (hr != S_OK) {
            mem_lock_bytes->Release();
            return {};
        }

        // Try to open storage
        IStorage* storage = nullptr;
        hr = StgOpenStorageOnILockBytes(mem_lock_bytes, nullptr,
            STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0,
            &storage);
        mem_lock_bytes->Release();

        if (FAILED(hr) || !storage) {
            return {};
        }

        storage->Release();
        return { "ole" };
    }

}  // namespace type_iden
