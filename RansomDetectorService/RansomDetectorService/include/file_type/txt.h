#pragma once
#include "../ulti/include.h"

#define BelowTextThreshold(part, total) (part <= total * 97 / 100)

namespace type_iden
{
	bool CheckPrintableUTF16(const span<unsigned char>& buffer);

	bool CheckPrintableUTF8(const span<unsigned char>& buffer);

	bool CheckPrintableUTF32(const span<unsigned char>& buffer);

	vector<string> GetTxtTypes(const span<UCHAR>& data);
}