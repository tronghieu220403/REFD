#include "ioctl.h"

NTSTATUS ioctl::DrvRegister(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path)
{
	DebugMessage("%ws", __FUNCTIONW__);

	driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ioctl::HandleIoctl;

	// routines that will execute once a handle to our device's symbolik link is opened/closed
	driver_object->MajorFunction[IRP_MJ_CREATE] = ioctl::MajorFunction;
	driver_object->MajorFunction[IRP_MJ_CLOSE] = ioctl::MajorFunction;

	NTSTATUS status = IoCreateDevice(driver_object, 0, &DEVICE_NAME, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &driver_object->DeviceObject);

	if (!NT_SUCCESS(status))
	{
		DebugMessage("Could not create device %wZ", DEVICE_NAME);
		return STATUS_UNSUCCESSFUL;
	}
	else
	{
		DebugMessage("Device %wZ created", DEVICE_NAME);
	}

	status = IoCreateSymbolicLink(&DEVICE_SYMBOLIC_NAME, &DEVICE_NAME);
	if (NT_SUCCESS(status))
	{
		DebugMessage("Symbolic link %wZ created", DEVICE_SYMBOLIC_NAME);
	}
	else
	{
		DebugMessage("Error creating symbolic link %wZ", DEVICE_SYMBOLIC_NAME);
		return STATUS_UNSUCCESSFUL;
	}
	return STATUS_SUCCESS;
}

NTSTATUS ioctl::DrvUnload(PDRIVER_OBJECT driver_object)
{
	DebugMessage("%ws", __FUNCTIONW__);

	IoDeleteSymbolicLink(&DEVICE_SYMBOLIC_NAME);
	IoDeleteDevice(driver_object->DeviceObject);

	return STATUS_SUCCESS;
}

NTSTATUS ioctl::HandleIoctl(PDEVICE_OBJECT device_object, PIRP irp)
{
	UNREFERENCED_PARAMETER(device_object);

	PIO_STACK_LOCATION stack_loc = IoGetCurrentIrpStackLocation(irp);
	stack_loc = IoGetCurrentIrpStackLocation(irp);

	IOCTL_CMD* cmd = (IOCTL_CMD*)irp->AssociatedIrp.SystemBuffer;

	// TODO: Check size of the system buffer

	bool status_reply = false;

	if (stack_loc->Parameters.DeviceIoControl.IoControlCode != IOCTL_HIEU)
	{
		return STATUS_UNSUCCESSFUL;
	}

	IOCTL_CMD_CLASS type = cmd->cmd_class;
	// TODO: Build a function to handle each type
	String<WCHAR> str;
	int pid;
	String<WCHAR> image_path;
	switch (type)
	{
	case IOCTL_CMD_CLASS::kGetImageFromPid:
		break;

	case IOCTL_CMD_CLASS::kEnableSelfDefense:
		status_reply = true;
		RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, &status_reply, sizeof(bool));
		break;
	case IOCTL_CMD_CLASS::kDisableSelfDefense:
		status_reply = true;
		RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, &status_reply, sizeof(bool));
		break;
	default:
		break;
	}

	irp->IoStatus.Status = STATUS_SUCCESS;

	IoCompleteRequest(irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS ioctl::MajorFunction(PDEVICE_OBJECT device_object, PIRP irp)
{
	UNREFERENCED_PARAMETER(device_object);

	PIO_STACK_LOCATION stackLocation = NULL;
	stackLocation = IoGetCurrentIrpStackLocation(irp);

	switch (stackLocation->MajorFunction)
	{
	case IRP_MJ_CREATE:
		DebugMessage("Handle to symbolink link %wZ opened", DEVICE_SYMBOLIC_NAME);
		break;
	case IRP_MJ_CLOSE:
		DebugMessage("Handle to symbolink link %wZ closed", DEVICE_SYMBOLIC_NAME);
		break;
	default:
		break;
	}

	irp->IoStatus.Information = 0;
	irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;

}