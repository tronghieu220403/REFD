#pragma once
#include "../../std/string/wstring.h"
#include "../../std/vector/vector.h"

std::WString GetProcessImageName(HANDLE pid);

LONGLONG GetNtSystemTime();
