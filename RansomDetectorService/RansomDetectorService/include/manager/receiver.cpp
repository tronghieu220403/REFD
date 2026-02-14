#include "receiver.h"
#include "ulti/file_helper.h"

namespace manager
{

    Receiver* Receiver::GetInstance()
    {
        static Receiver instance;
        return &instance;
    }

    bool Receiver::Init() {

        return true;
    }

    void Receiver::Uninit() {
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
            file_io_info = std::move(file_io_queue_.front());
            file_io_queue_.pop();
        }
        return file_io_info;
    }

    ull Receiver::GetQueueSize() {
        return file_io_queue_.size();
    }

    void Receiver::MoveQueueSync(std::queue<FileIoInfo>& target_file_io_queue) {
        std::lock_guard<std::mutex> lk(file_io_mutex_);
        target_file_io_queue = std::move(file_io_queue_);
    }

    void Receiver::PushFileEventSync(const std::wstring& path, ULONG pid) {
        std::lock_guard<std::mutex> lk(file_io_mutex_);
        if (path.size() == 0) {
            return;
        }
        FileIoInfo info;
        info.path = helper::GetLongDosPath(helper::GetDosPath(path));
        info.pid = pid;
        if (info.path.size() == 0) {
            return;
        }
        file_io_queue_.push(std::move(info));
        return;
    }
}