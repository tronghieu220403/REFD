#ifndef FILE_TYPE_IMAGE_H_
#define FILE_TYPE_IMAGE_H_
#include "../ulti/include.h"

namespace type_iden
{
	vector<string> GetPngTypes(const span<UCHAR>& data);
	vector<string> GetJpgTypes(const std::span<UCHAR>& data);
	vector<string> GetWebpTypes(const std::span<UCHAR>& data);
}

#endif // FILE_TYPE_IMAGE_H_