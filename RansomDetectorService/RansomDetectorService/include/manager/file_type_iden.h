#pragma once
#define _CRT_SECURE_NO_DEPRECATE
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#ifndef ETWSERVICE_MANAGER_FILE_TYPE_IDEN_H_
#define ETWSERVICE_MANAGER_FILE_TYPE_IDEN_H_

#include "../ulti/include.h"
#define TRID_PATH L"??"

#define BelowTextThreshold(part, total) (part <= total * 97 / 100)

namespace type_iden
{
	bool CheckPrintableUTF16(const std::vector<unsigned char>& buffer);

	bool CheckPrintableUTF8(const std::vector<unsigned char>& buffer);

	bool CheckPrintableANSI(const std::vector<unsigned char>& buffer);

	// TrID lib can only run on x86 process. Do not use this in async multi-thread.
	// To use TrID on async multi-thread, each instance of TrID must create and load an unique clone of TrIDLib.dll (for example TrIDLib_X.dll)
	class TrID
	{
	private:

	public:

	};
}

#endif
