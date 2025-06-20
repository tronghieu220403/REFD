#pragma once

#include <Ntifs.h>
#include <fltKernel.h>

#include "../std/vector/vector.h"
#include "../com/ioctl/ioctl.h"
#include "../com/comport/comport.h"
#include "debug.h"
#include "MiniFs.h"

namespace reg
{
	struct IrpMjFunc
	{
		ull irp_mj_function_code = 0;
		PFLT_PRE_OPERATION_CALLBACK pre_func = nullptr;
		PFLT_POST_OPERATION_CALLBACK post_func = nullptr;
	};

	struct Context
	{
		Vector<FLT_PREOP_CALLBACK_STATUS>* status = nullptr;
        Vector<PVOID>* completion_context = nullptr;
	};

	extern Vector<IrpMjFunc>* kFltFuncVector;

	extern Vector<void*>* kDrvFuncVector;

	void DrvRegister(
		_In_ PDRIVER_OBJECT driver_object,
		_In_ PUNICODE_STRING registry_path
	);

	void FltRegister();

	void PostFltRegister();

	void DrvUnload(PDRIVER_OBJECT driver_object);

	NTSTATUS FltUnload();

	Context* AllocCompletionContext();

	void DeallocCompletionContext(Context*);
}

