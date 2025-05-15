#ifndef MINIFILTER_COMM_H
#define MINIFILTER_COMM_H

#include <Windows.h>
#include <fltuser.h>
#include <string>
#include <vector>

class FltComPort {
public:
	FltComPort();
	~FltComPort();

	// Creates the communication port
	HRESULT Create(const std::wstring& port_name);

	// Receives a message from the communication port into a buffer
	HRESULT Get(PFILTER_MESSAGE_HEADER buffer, size_t size);

	// Replies to the received message with a buffer
	HRESULT Reply(const PFILTER_REPLY_HEADER buffer, size_t size);

private:
	HANDLE h_port_;  // Handle for the communication port
};

#endif  // MINIFILTER_COMM_H
