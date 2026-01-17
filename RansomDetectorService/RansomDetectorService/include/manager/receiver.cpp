#include "receiver.h"
#include "file_helper.h"

namespace manager
{
    Receiver* Receiver::instance_ = nullptr;
    std::mutex Receiver::instance_mutex_;

    Receiver* Receiver::GetInstance() {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_)
            instance_ = new Receiver();
        return instance_;
    }

    void Receiver::DeleteInstance() {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (instance_)
        {
            instance_->Uninit();
            delete instance_;
            instance_ = nullptr;
        }
    }

    bool Receiver::Init() {
        if (running_)
            return true;

        running_ = true;
        collector_thread_ = std::thread(ReceiverThread);
        return true;
    }

    void Receiver::Uninit() {
        running_ = false;

        if (collector_thread_.joinable())
            collector_thread_.join();
    }

    void Receiver::LockMutex() {
        file_io_mutex_.lock();
    }

    void Receiver::UnlockMutex() {
        file_io_mutex_.unlock();
    }

    FileIoInfo Receiver::PopFileIoEvent() {
        FileIoInfo file_io_info;
        if (!file_io_queue_.empty())
        {
            file_io_info = file_io_queue_.front();
            file_io_queue_.pop();
        }
        return file_io_info;
    }

    ull Receiver::GetQueueSize() {
        return file_io_queue_.size();
    }

    void Receiver::MoveQueue(std::queue<FileIoInfo>& target_file_io_queue) {
        target_file_io_queue = std::move(file_io_queue_);
    }

    void Receiver::PushFileEvent(std::wstring& path) {
        FileIoInfo info;
        info.path = std::move(path);
        file_io_queue_.push(std::move(info));
        return;
    }

    void Receiver::PushFileEventFromBuffer(
        const void* buffer,
        size_t buffer_size)
    {
        if (file_io_queue_.size() >= MAX_QUEUE_SIZE)
            return;

        if (buffer_size <= sizeof(FILTER_MESSAGE_HEADER))
            return;

        const uint8_t* ptr = (const uint8_t*)buffer;

        // skip FILTER_MESSAGE_HEADER
        ptr += sizeof(FILTER_MESSAGE_HEADER);

        size_t string_bytes = buffer_size - sizeof(FILTER_MESSAGE_HEADER);

        if (string_bytes < sizeof(wchar_t))
            return;

        if (string_bytes % sizeof(wchar_t) != 0)
            return;

        size_t wchar_len = string_bytes / sizeof(wchar_t);

        const wchar_t* raw_path = (const wchar_t*)ptr;

        size_t actual_len = 0;
        for (; actual_len < wchar_len; actual_len++)
        {
            if (raw_path[actual_len] == L'\0')
                break;
        }

        if (actual_len == 0)
            return;

        std::wstring path(raw_path, actual_len);

        FileIoInfo info;
        info.path = helper::GetLongDosPath(helper::GetDosPath(path));

        file_io_queue_.push(std::move(info));
    }

	void Receiver::ReceiverThread()
	{
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        auto rcv = Receiver::GetInstance();
        if (rcv == nullptr) {
            return;
        }

		// Create a communication port
		FltComPort com_port;

        constexpr size_t MAX_BUFFER_SIZE = 64 * 1024;
        std::vector<uint8_t> buffer(MAX_BUFFER_SIZE);

        while (true)
        {
            if (rcv->running_ == false) {
                return;
            }
            HRESULT hr = com_port.Create(PORT_NAME);
            if (FAILED(hr))
            {
                PrintDebugW(L"[FileIo] Create port failed: 0x%08X", hr);
                Sleep(1000);
                continue;
            }

            while (true)
            {
                if (rcv->running_ == false) {
                    return;
                }

                hr = com_port.Get(
                    (PFILTER_MESSAGE_HEADER)buffer.data(),
                    buffer.size()
                );

                if (FAILED(hr))
                    break;

                auto hdr = (PFILTER_MESSAGE_HEADER)buffer.data();
                size_t msg_size = hdr->ReplyLength;

                if (msg_size == 0 || msg_size > buffer.size())
                    continue;

                rcv->LockMutex();
                rcv->PushFileEventFromBuffer(buffer.data(), msg_size);
                rcv->UnlockMutex();
            }
        }
	}
}