#pragma once
#include "../ulti/include.h"

namespace type_iden
{
	vector<string> GetGzipTypes(const std::span<UCHAR>& data);
}