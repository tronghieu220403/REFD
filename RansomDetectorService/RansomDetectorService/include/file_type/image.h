#pragma once
#include "../ulti/include.h"

namespace type_iden
{
	vector<string> GetPngTypes(const span<UCHAR>& data);
	vector<string> GetJpgTypes(const std::span<UCHAR>& data);
	vector<string> GetWebpTypes(const std::span<UCHAR>& data);
}