#include "../template/MiniFs.h"
#include "../com/comport/comport.h"
#include "../std/string/wstring.h"
//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION kCallbacks[] = {

    { IRP_MJ_CREATE,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },
/*
    { IRP_MJ_CREATE_NAMED_PIPE,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },
*/
    { IRP_MJ_CLOSE,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },
/*
    { IRP_MJ_READ,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },
*/
    { IRP_MJ_WRITE,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },
/*
    { IRP_MJ_QUERY_INFORMATION,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },
*/
    { IRP_MJ_SET_INFORMATION,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },
/*
    { IRP_MJ_QUERY_EA,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_SET_EA,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_FLUSH_BUFFERS,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_QUERY_VOLUME_INFORMATION,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_SET_VOLUME_INFORMATION,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_DIRECTORY_CONTROL,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_FILE_SYSTEM_CONTROL,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_DEVICE_CONTROL,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_INTERNAL_DEVICE_CONTROL,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_SHUTDOWN,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      NULL },                               //post operations not supported

    { IRP_MJ_LOCK_CONTROL,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_CLEANUP,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_CREATE_MAILSLOT,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_QUERY_SECURITY,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_SET_SECURITY,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_QUERY_QUOTA,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_SET_QUOTA,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_PNP,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },
*/
    { IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },
/*
    { IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_ACQUIRE_FOR_MOD_WRITE,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_RELEASE_FOR_MOD_WRITE,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_ACQUIRE_FOR_CC_FLUSH,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_RELEASE_FOR_CC_FLUSH,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_NETWORK_QUERY_OPEN,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_MDL_READ,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_MDL_READ_COMPLETE,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_PREPARE_MDL_WRITE,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_MDL_WRITE_COMPLETE,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_VOLUME_MOUNT,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },

    { IRP_MJ_VOLUME_DISMOUNT,
      0,
      (PFLT_PRE_OPERATION_CALLBACK)&MiniFsPreOperation,
      (PFLT_POST_OPERATION_CALLBACK)&MiniFsPostOperation },
*/
    { IRP_MJ_OPERATION_END }
};


CONST FLT_CONTEXT_REGISTRATION kContexts[] = {
	{ FLT_STREAMHANDLE_CONTEXT, 0, MiniFsContextCleanup, 0,'HNTC'},
	{ FLT_FILE_CONTEXT, 0 , MiniFsContextCleanup, 0, 'HNTC'},
	{ FLT_CONTEXT_END }
};

//
//  This defines what we want to filter with FltMgr
//

FLT_REGISTRATION kFilterRegistration = {

	sizeof(FLT_REGISTRATION),			//  Size
	FLT_REGISTRATION_VERSION,           //  Version
	0,									//  flags

	kContexts,                               //  Context
	kCallbacks,                          //  Operation callbacks

	(PFLT_FILTER_UNLOAD_CALLBACK)MiniFsUnload,                                  //  MiniFilterUnload

	(PFLT_INSTANCE_SETUP_CALLBACK)MiniFsInstanceSetup,                           //  InstanceSetup
	(PFLT_INSTANCE_QUERY_TEARDOWN_CALLBACK)MiniFsInstanceQueryTeardown,         //  InstanceQueryTeardown (called via FltDetachVolume, not FilterUnload)
	(PFLT_INSTANCE_TEARDOWN_CALLBACK)MiniFsInstanceTeardownStart,               //  InstanceTeardownStart
	(PFLT_INSTANCE_TEARDOWN_CALLBACK)MiniFsInstanceTeardownComplete,            //  InstanceTeardownComplete

	NULL,                               //  GenerateFileName
	NULL,                               //  GenerateDestinationFileName
	NULL                                //  NormalizeNameComponent

};

NTSTATUS
MiniFsInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS flt_objects,
	_In_ FLT_INSTANCE_SETUP_FLAGS flags,
	_In_ DEVICE_TYPE volume_device_type,
	_In_ FLT_FILESYSTEM_TYPE volume_filesystem_type
)
{
	UNREFERENCED_PARAMETER(flt_objects);
	UNREFERENCED_PARAMETER(flags);
	UNREFERENCED_PARAMETER(volume_device_type);
	UNREFERENCED_PARAMETER(volume_filesystem_type);

	PAGED_CODE();

	return STATUS_SUCCESS;
}


NTSTATUS
MiniFsInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS flt_objects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS flags
)
{
	UNREFERENCED_PARAMETER(flt_objects);
	UNREFERENCED_PARAMETER(flags);

	PAGED_CODE();

	return STATUS_SUCCESS;
}


VOID
MiniFsInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS flt_objects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS flags
)
{
	UNREFERENCED_PARAMETER(flt_objects);
	UNREFERENCED_PARAMETER(flags);

	PAGED_CODE();

}


VOID
MiniFsInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS flt_objects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS flags
)
{
	UNREFERENCED_PARAMETER(flt_objects);
	UNREFERENCED_PARAMETER(flags);

	PAGED_CODE();

}

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT driver_object,
	_In_ PUNICODE_STRING registry_path
)
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER(registry_path);

	DebugMessage("SelfDefenseKernel");

	DebugMessage("GetSystemRoutineAddresses");
	GetSystemRoutineAddresses();

	DebugMessage("DrvRegister");
	reg::DrvRegister(driver_object, registry_path);

	driver_object->DriverUnload = (PDRIVER_UNLOAD)DriverUnload;

	DebugMessage("FltRegister");
	reg::FltRegister();

	//
	//  Register with FltMgr to tell it our callback routines
	//

	// Block all mandatory unload (sc stop, net stop, Win32 ControlService function)
	kFilterRegistration.Flags = FLTFL_REGISTRATION_DO_NOT_SUPPORT_SERVICE_STOP;

	status = FltRegisterFilter(driver_object,
		&kFilterRegistration,
		&kFilterHandle);

	//FLT_ASSERT(NT_SUCCESS(status));

	if (NT_SUCCESS(status)) {

		//
		//  Start filtering i/o
		//

		status = FltStartFiltering(kFilterHandle);

		if (!NT_SUCCESS(status)) {

			FltUnregisterFilter(kFilterHandle);
		}
	}

	reg::PostFltRegister();

	return status;
}

NTSTATUS
DriverUnload(
	PDRIVER_OBJECT driver_object
)
{
	DebugMessage("Driver unload callback routine");
	reg::DrvUnload(driver_object);

	DebugMessage("Successfully unloaded driver");
	return STATUS_SUCCESS;
}

NTSTATUS
MiniFsUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS flags
)
{
	UNREFERENCED_PARAMETER(flags);

	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;

	status = reg::FltUnload();
	if (status == STATUS_FLT_DO_NOT_DETACH)
	{
		DebugMessage("Do not unload: STATUS_FLT_DO_NOT_DETACH");
		return STATUS_FLT_DO_NOT_DETACH;
	}

	DebugMessage("Successfully unloaded filter");

	return STATUS_SUCCESS;
}

FLT_PREOP_CALLBACK_STATUS
MiniFsPreOperation(
	_Inout_ PFLT_CALLBACK_DATA data,
	_In_ PCFLT_RELATED_OBJECTS flt_objects,
	_Flt_CompletionContext_Outptr_ PVOID* completion_context
)
{
	PAGED_CODE();

	if (flt_objects->FileObject == NULL)
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	reg::Context* p = nullptr;
	if ((*completion_context) == nullptr)
	{
		p = reg::AllocCompletionContext();
		(*completion_context) = (PVOID*)p;
		if (p == nullptr)
		{
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}
		p->status->Resize(reg::kFltFuncVector->Size());
	}
	else
	{
		p = (reg::Context*)(*completion_context);
	}

	for (int i = 0; i < reg::kFltFuncVector->Size(); i++)
	{
		if (data->Iopb->MajorFunction == (*reg::kFltFuncVector)[i].irp_mj_function_code &&
			(*reg::kFltFuncVector)[i].pre_func != nullptr)
		{
			PVOID completion_context_tmp = nullptr;
			FLT_PREOP_CALLBACK_STATUS status = (*reg::kFltFuncVector)[i].pre_func(data, flt_objects, &completion_context_tmp);
			(*(p->status))[i] = status;
			(*p->completion_context)[i] = completion_context_tmp;
			if (status == FLT_PREOP_COMPLETE)
			{
				DeallocCompletionContext(p);
				return FLT_PREOP_COMPLETE;
			}
		}
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
MiniFsPostOperation(
	_Inout_ PFLT_CALLBACK_DATA data,
	_In_ PCFLT_RELATED_OBJECTS flt_objects,
	_In_opt_ PVOID completion_context,
	_In_ FLT_POST_OPERATION_FLAGS flags
)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(flt_objects);
	UNREFERENCED_PARAMETER(completion_context);
	UNREFERENCED_PARAMETER(flags);

	reg::Context* p = (reg::Context*)completion_context;
	if (p == nullptr)
	{
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	if (flags & FLTFL_POST_OPERATION_DRAINING)
	{
		DeallocCompletionContext(p);
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	if (!NT_SUCCESS(data->IoStatus.Status))
	{
		DeallocCompletionContext(p);
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	FLT_POSTOP_CALLBACK_STATUS postop_status = FLT_POSTOP_FINISHED_PROCESSING;

	for (int i = 0; i < (*reg::kFltFuncVector).Size(); i++)
	{
		if (data->Iopb->MajorFunction == (*reg::kFltFuncVector)[i].irp_mj_function_code &&
			(*reg::kFltFuncVector)[i].post_func != nullptr)
		{
			if ((*(p->status))[i] != FLT_PREOP_SUCCESS_NO_CALLBACK && (*(p->status))[i] != FLT_PREOP_COMPLETE)
			{
				PVOID completion_context_tmp = (*p->completion_context)[i];
				auto ret_status = (*reg::kFltFuncVector)[i].post_func(data, flt_objects, completion_context_tmp, flags);
				if (ret_status == FLT_POSTOP_MORE_PROCESSING_REQUIRED)
				{
					postop_status = FLT_POSTOP_MORE_PROCESSING_REQUIRED;
				}
			}
		}
	}

	DeallocCompletionContext(p);
	return postop_status;
}

void MiniFsContextCleanup(PFLT_CONTEXT context, FLT_CONTEXT_TYPE context_type)
{
	// Must define size of context in kContexts to registering in minifilter driver
	if (context_type == FLT_STREAMHANDLE_CONTEXT)
	{

	}
	else if (context_type == FLT_FILE_CONTEXT)
	{

	}
	else
	{

	}
	return;
}

