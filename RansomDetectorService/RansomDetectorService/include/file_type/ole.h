#pragma once
#include "../ulti/include.h"

namespace type_iden
{
	vector<string> GetOleTypes(const std::span<UCHAR>& data);
}