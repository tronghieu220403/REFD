#include "register.h"
#include "../function/collector.h"

namespace reg
{
	Vector<IrpMjFunc>* kFltFuncVector = nullptr;

	Vector<void*>* kDrvFuncVector = nullptr;

	void DrvRegister(
		_In_ PDRIVER_OBJECT driver_object,
		_In_ PUNICODE_STRING registry_path
	)
	{
		DebugMessage("DriverRegister");

		kDrvFuncVector = new Vector<void*>();

		//ioctl::DrvRegister(driver_object, registry_path);
		collector::DrvRegister();
		
		return;
	}


	void DrvUnload(PDRIVER_OBJECT driver_object)
	{
		DebugMessage("%ws", __FUNCTIONW__);

		collector::DrvUnload();
		//ioctl::DrvUnload(driver_object);

		delete kDrvFuncVector;
		kDrvFuncVector = nullptr;

		return;
	}


	void FltRegister()
	{
		DebugMessage("MiniFilterRegister");

		kFltFuncVector = new Vector<IrpMjFunc>();
		
		com::kComPort = new com::ComPort();

		collector::FltRegister();

		return;
	}

	void PostFltRegister()
	{
		com::kComPort->SetPfltFilter(kFilterHandle);
		com::kComPort->Create();
	}

	void FltUnload()
	{
		DebugMessage("%ws", __FUNCTIONW__);
        DebugMessage("Closing com port");
		com::kComPort->Close();
        DebugMessage("Unregistering kFilterHandle");
		FltUnregisterFilter(kFilterHandle);

        DebugMessage("Unregistering filter");
		collector::FltUnload();

        DebugMessage("Free memory structures");
		delete kFltFuncVector;
		kFltFuncVector = nullptr;
		delete com::kComPort;
        com::kComPort = nullptr;

		DebugMessage("Done %s", __FUNCTION__);
		return;
	}

	Context* AllocCompletionContext()
	{
		// DebugMessage("Alloccompletion_context");

		Context* context = new Context();
		context->status = new Vector<FLT_PREOP_CALLBACK_STATUS>(kFltFuncVector->Size());
        context->completion_context = new Vector<PVOID>(kFltFuncVector->Size());
		return context;
	}

	void DeallocCompletionContext(Context *context)
	{
		// DebugMessage("Dealloccompletion_context");

        delete context->completion_context;
		delete context->status;
		delete context;
	}

}