#ifndef FILE_TYPE_TXT_H_
#define FILE_TYPE_TXT_H_
#include "../ulti/include.h"

#define BelowTextThreshold(part, total) (part <= total * 97 / 100)

namespace type_iden
{
	bool CheckPrintableUTF16(const span<unsigned char>& buffer);

	bool CheckPrintableUTF8(const span<unsigned char>& buffer);

	bool CheckPrintableUTF32(const span<unsigned char>& buffer);

	vector<string> GetTxtTypes(const span<UCHAR>& data);
}

#endif // FILE_TYPE_TXT_H_