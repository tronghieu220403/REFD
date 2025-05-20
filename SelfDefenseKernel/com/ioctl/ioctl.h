#pragma once

#include "../../std/memory/memory.h"
#include "../../std/string/string.h"
#include "../../std/vector/vector.h"
#include "../../std/sync/mutex.h"
#include "../../template/register.h"
#include "../../template/flt-ex.h"
#include "../common.h"
#include "../../function/query.h"
#include "../../function/self_defense.h"

inline UNICODE_STRING DEVICE_NAME = RTL_CONSTANT_STRING(L"\\Device\\HieuDevice");
inline UNICODE_STRING DEVICE_SYMBOLIC_NAME = RTL_CONSTANT_STRING(L"\\??\\HieuDeviceLink");

#define IOCTL_HIEU CTL_CODE(FILE_DEVICE_UNKNOWN, 0x2204, METHOD_BUFFERED, FILE_ANY_ACCESS)

namespace ioctl
{
	NTSTATUS DrvRegister(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path);
	NTSTATUS DrvUnload(PDRIVER_OBJECT driver_object);

	NTSTATUS HandleIoctl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

	NTSTATUS MajorFunction(PDEVICE_OBJECT device_object, PIRP irp);

}
