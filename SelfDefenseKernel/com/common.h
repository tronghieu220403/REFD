#pragma once

#include "../../std/memory/memory.h"
#include "../../std/string/string.h"
#include "../../std/vector/vector.h"
#include "../../std/sync/mutex.h"

namespace ioctl
{
	enum IOCTL_CMD_CLASS
	{
		kGetImageFromPid = 0x80002000,
		kEnableSelfDefense = 0x80002001,
		kDisableSelfDefense = 0x80002002,
	};

	struct IOCTL_CMD_GET_IMAGE_FROM_PID
	{
		ULONG pid;
	};
	typedef struct IOCTL_CMD_GET_IMAGE_FROM_PID* PIOCTL_CMD_GET_IMAGE_FROM_PID;

	struct IOCTL_CMD_ENABLE_SELF_DEFENSE
	{
	};
	typedef struct IOCTL_CMD_ENABLE_SELF_DEFENSE* PIOCTL_CMD_ENABLE_SELF_DEFENSE;

	struct IOCTL_CMD_DISABLE_SELF_DEFENSE
	{
	};
	typedef struct IOCTL_CMD_DISABLE_SELF_DEFENSE* PIOCTL_CMD_DISABLE_SELF_DEFENSE;

	struct IOCTL_CMD
	{
		IOCTL_CMD_CLASS cmd_class;
		size_t data_len;
		char data[1];

		IOCTL_CMD_GET_IMAGE_FROM_PID ParseGetImageFromPid()
		{
			if (cmd_class != IOCTL_CMD_CLASS::kGetImageFromPid)
			{
				return IOCTL_CMD_GET_IMAGE_FROM_PID();
			}
			ULONG pid = *(ULONG*)data;
			return IOCTL_CMD_GET_IMAGE_FROM_PID{ pid };
		}
	};

	/*
	Write muliple functions to flatten all the above structures of IOCTL_CMD_CLASS into an IOCTL_CMD consecutive array of char, so that I can send it through a communication port in kernel mode driver. The function take only one parameter: the IOCTL_CMD_xxx structure. First we must allocate memory for IOCTL_CMD structure (the remaining data is write right after the data[1]). Return a pointer to that IOCTL_CMD.
	*/

	// User must free the returned buffer.
	inline IOCTL_CMD* FlattenIoctlCmd(IOCTL_CMD_CLASS query_class, const String<WCHAR>& str)
	{
		IOCTL_CMD* ioctl_cmd = (IOCTL_CMD*)new char[sizeof(IOCTL_CMD) + str.Size() * sizeof(WCHAR)];
		ioctl_cmd->cmd_class = query_class;
		ioctl_cmd->data_len = str.Size() * sizeof(WCHAR);
		::MemCopy((char*)ioctl_cmd->data, (char*)str.Data(), ioctl_cmd->data_len);
		return ioctl_cmd;
	}

	inline void WriteIoctlCmd(IOCTL_CMD* ioctl_cmd, IOCTL_CMD_CLASS query_class, const String<WCHAR>& str)
	{
		ioctl_cmd->cmd_class = query_class;
		ioctl_cmd->data_len = str.Size() * sizeof(WCHAR);
		::MemCopy((char*)ioctl_cmd->data, (char*)str.Data(), ioctl_cmd->data_len);
	}
}