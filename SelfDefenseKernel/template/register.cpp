#include "register.h"

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
		self_defense::DrvRegister();
		
		return;
	}


	void DrvUnload(PDRIVER_OBJECT driver_object)
	{
		DebugMessage("%ws", __FUNCTIONW__);

		self_defense::DrvUnload();
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

		self_defense::FltRegister();

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
		com::kComPort->Close();
		FltUnregisterFilter(kFilterHandle);

		self_defense::FltUnload();

		delete kFltFuncVector;
		kFltFuncVector = nullptr;

		return;
	}

	Context* AllocCompletionContext()
	{
		// DebugMessage("AllocCompletionContext");

		Context* context = new Context();
		context->status = new Vector<FLT_PREOP_CALLBACK_STATUS>(kFltFuncVector->Size());

		return context;

	}

	void DeallocCompletionContext(Context *context)
	{
		// DebugMessage("DeallocCompletionContext");

		delete context->status;

		delete context;
	}

}