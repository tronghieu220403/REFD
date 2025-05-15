#include "minifilter_comm.h"
#include <iostream>
#include <thread>

// Constructor
FltComPort::FltComPort() : h_port_(NULL) {}

// Destructor
FltComPort::~FltComPort() {
	if (h_port_ != NULL) {
		CloseHandle(h_port_);
	}
}

// Creates the communication port
HRESULT FltComPort::Create(const std::wstring& port_name) {
	HRESULT hr = FilterConnectCommunicationPort(port_name.c_str(), 0, NULL, 0, NULL, &h_port_);
	return hr;
}

// Receives a message from the communication port into a buffer
HRESULT FltComPort::Get(PFILTER_MESSAGE_HEADER buffer, size_t size) {
	return FilterGetMessage(h_port_, buffer, static_cast<DWORD>(size), NULL);
}

// Replies to the received message with a buffer
HRESULT FltComPort::Reply(const PFILTER_REPLY_HEADER buffer, size_t size) {
	return FilterReplyMessage(h_port_, (PFILTER_REPLY_HEADER)buffer, static_cast<DWORD>(size));
}
