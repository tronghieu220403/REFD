#include "comport.h"

namespace com
{
	NTSTATUS ComPort::Create()
	{
		NTSTATUS status;
		UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\hieunt_mf");

		status = FltBuildDefaultSecurityDescriptor(&sec_des_, FLT_PORT_ALL_ACCESS);
		if (NT_SUCCESS(status)) {

			InitializeObjectAttributes(&obj_attr_, &name, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, sec_des_);
			status = FltCreateCommunicationPort(p_filter_handle_, &server_port_, &obj_attr_, NULL,
				(PFLT_CONNECT_NOTIFY)ComPort::ConnectHandler,
				(PFLT_DISCONNECT_NOTIFY)ComPort::DisonnectHandler,
				(PFLT_MESSAGE_NOTIFY)ComPort::SendRecvHandler,
				1 // MaxConnections
			);

			if (NT_SUCCESS(status))
			{
				DebugMessage("ComPort creation: oke.");
				return status;
			}
			else
			{
				DebugMessage("FltCreateCommunicationPort failed: %x", status);
				ComPort::Close();
			}
		}
		else
		{
			DebugMessage("FltBuildDefaultSecurityDescriptor failed: %x", status);
		}
		return status;
	}

	NTSTATUS ComPort::Send(PVOID sender_buffer, ULONG sender_buffer_length, PVOID reply_buffer, ULONG reply_buffer_length)
	{
		if (client_port_ == nullptr)
		{
			DebugMessage("client_port_ is null");
			return STATUS_CONNECTION_INVALID;
		}
		LARGE_INTEGER timeout;
		timeout.QuadPart = -150000000; // 15s
		NTSTATUS status = FltSendMessage(p_filter_handle_,
			&client_port_,
			sender_buffer,
			sender_buffer_length,
			reply_buffer,
			&reply_buffer_length,
			&timeout
		);
		if (status != STATUS_SUCCESS && status != 0x11)
		{
			DebugMessage("SendMessage failed: %x", status);
		}
		else
		{
			DebugMessage("SendMessage success: %x", status);
			status = STATUS_SUCCESS;
		}
		return status;
	}

	NTSTATUS ComPort::Close()
	{
		if (sec_des_ != NULL)
		{
			FltFreeSecurityDescriptor(sec_des_);
			sec_des_ = NULL;
		}

		if (server_port_ != NULL)
		{
			FltCloseCommunicationPort(server_port_);
			server_port_ = nullptr;
		}
		return STATUS_SUCCESS;
	}

	NTSTATUS ComPort::ConnectHandler(PFLT_PORT client_port, PVOID server_port_cookie, PVOID connection_context, ULONG size_of_context, PVOID* connection_port_cookie)
	{
		UNREFERENCED_PARAMETER(server_port_cookie);
		UNREFERENCED_PARAMETER(connection_context);
		UNREFERENCED_PARAMETER(size_of_context);
		UNREFERENCED_PARAMETER(connection_port_cookie);

		client_port_ = client_port;

		DebugMessage("ComPort::ConnectHandler connected, client port: %p", client_port_);

		return STATUS_SUCCESS;
	}

	VOID ComPort::DisonnectHandler(PVOID connection_cookie)
	{
		UNREFERENCED_PARAMETER(connection_cookie);

		FltCloseClientPort(p_filter_handle_, &client_port_);

		// DebugMessage("Disonnected");

		client_port_ = nullptr;

		return;
	}

	// The Filter Manager calls this routine, at IRQL = PASSIVE_LEVEL, whenever a user-mode application calls FilterSendMessage to send a message to the minifilter driver through the client port. 
	NTSTATUS ComPort::SendRecvHandler(PVOID port_cookie, PVOID input_buffer, ULONG input_buffer_length, PVOID output_buffer, ULONG output_buffer_length, PULONG return_output_buffer_length)
	{
		UNREFERENCED_PARAMETER(port_cookie);
		UNREFERENCED_PARAMETER(input_buffer);
		UNREFERENCED_PARAMETER(input_buffer_length);
		UNREFERENCED_PARAMETER(output_buffer);
		UNREFERENCED_PARAMETER(output_buffer_length);
		UNREFERENCED_PARAMETER(return_output_buffer_length);

		NTSTATUS status = STATUS_SUCCESS;

		PAGED_CODE();

		DebugMessage("output_buffer_length %d", output_buffer_length);

		if (!IS_ALIGNED(output_buffer, sizeof(char)))
		{
			status = STATUS_DATATYPE_MISALIGNMENT;
			return status;
		}

		__try {

			ProbeForWrite(output_buffer, output_buffer_length, TYPE_ALIGNMENT(char));

		}
		__except (EXCEPTION_EXECUTE_HANDLER) {

			return GetExceptionCode();
		}

		PCHAR msg = (char*)"";

		RtlCopyMemory(&output_buffer, msg, sizeof(msg));

		__try {

			ProbeForRead(input_buffer, input_buffer_length, TYPE_ALIGNMENT(char));

		}
		__except (EXCEPTION_EXECUTE_HANDLER) {

			return GetExceptionCode();
		}

		return STATUS_SUCCESS;
	}

	void ComPort::SetPfltFilter(PFLT_FILTER p_filter_handle)
	{
		p_filter_handle_ = p_filter_handle;
	}

	PFLT_FILTER ComPort::GetPfltFilter()
	{
		return p_filter_handle_;
	}

}