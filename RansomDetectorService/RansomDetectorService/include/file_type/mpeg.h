#pragma once
#include "../ulti/include.h"

namespace type_iden
{
	vector<string> GetMpegTypes(const std::span<UCHAR>& data);
}