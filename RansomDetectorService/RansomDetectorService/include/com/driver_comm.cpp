#include "driver_comm.h"

DriverComm::DriverComm() : device(INVALID_HANDLE_VALUE), status(FALSE) {}

DriverComm::DriverComm(const std::wstring& device_link)
{
    this->Initialize(device_link);
}

bool DriverComm::Initialize(const std::wstring& device_link) {
    device = CreateFileW(device_link.c_str(), GENERIC_WRITE | GENERIC_READ | GENERIC_EXECUTE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, 0);
    if (device == INVALID_HANDLE_VALUE) {
        return false;
    }
    return true;
}

void DriverComm::Cleanup() {
    if (device != INVALID_HANDLE_VALUE) {
        CloseHandle(device);
        device = INVALID_HANDLE_VALUE;
    }
}

std::wstring DriverComm::GetProcessImageFromPid(int pid)
{
    std::vector<UCHAR> reply = SendCommand(IOCTL_CMD_CLASS::kGetImageFromPid, pid);
    if (reply.size() > 0) {
        return std::wstring(reinterpret_cast<wchar_t*>(reply.data()), reply.size() / sizeof(wchar_t));
    }
    return std::wstring();
}

std::vector<UCHAR> DriverComm::SendCommand(IOCTL_CMD_CLASS cmdClass, uint64_t number) {
    DWORD bytes_returned = 0;
    std::vector<UCHAR> reply(4096, 0);

    std::vector<char> in_buffer(sizeof(IOCTL_CMD) + sizeof(number));

    IOCTL_CMD* cmd = reinterpret_cast<IOCTL_CMD*>(&in_buffer[0]);
    cmd->cmd_class = cmdClass;
    cmd->data_len = sizeof(number);
    memcpy(cmd->data, &number, sizeof(number));

    BOOL status = DeviceIoControl(device, IOCTL_HIEU, &in_buffer[0], static_cast<DWORD>(in_buffer.size()), reply.data(), static_cast<DWORD>(reply.size()), &bytes_returned, nullptr);

    if (status == FALSE) {
        return {};  // Return an empty vector on failure
    }

    reply.resize(bytes_returned);  // Adjust the size to the actual data returned
    return reply;
}
