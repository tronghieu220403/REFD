#ifndef FILE_TYPE_AUDIOVIDEO_H_
#define FILE_TYPE_AUDIOVIDEO_H_
#include "../ulti/include.h"

namespace type_iden
{
	vector<string> GetAudioVideoTypes(const std::span<UCHAR>& data);
}
#endif // FILE_TYPE_AUDIOVIDEO_H_