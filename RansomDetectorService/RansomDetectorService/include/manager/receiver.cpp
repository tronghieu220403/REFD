#include "receiver.h"
#include "ulti/file_helper.h"

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

    void Receiver::MoveQueue(std::queue<FileIoInfo>& target_file_io_queue) {
        target_file_io_queue = std::move(file_io_queue_);
    }

    void Receiver::PushFileEvent(std::wstring& path) {
        FileIoInfo info;
        info.path = helper::GetLongDosPath(helper::GetDosPath(path));
        file_io_queue_.push(std::move(info));
        return;
    }
}