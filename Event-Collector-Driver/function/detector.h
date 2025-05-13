#pragma once
#ifndef DETECTOR_H
#define DETECTOR_H

#define BEGIN_WIDTH 1024
#define END_WIDTH 1024
#define MAX_PATH (260 * 4)

#include "../template/debug.h"
#include "../../std/string/string.h"

namespace detector
{
    static inline const WCHAR* kMonitoredExtension[] = {
        L".exe",
        L".dll",
        L".ics",
        L".powershell",
        L".zlib",
        L".apk",
        L".oxps",
        L".elf",
        L".webp",
        L".json",
        L".gzip",
        L".epub",
        L".tar",
        L".css",
        L".html",
        L".xml",
        L".mkv",
        L".javascript",
        L".mp3",
        L".csv",
        L".ods",
        L".rar",
        L".txt",
        L".eps",
        L".tif",
        L".svg",
        L".bmp",
        L".mp4",
        L".dwg",
        L".gif",
        L".pptx",
        L".ppt",
        L".xls",
        L".xlsx",
        L".doc",
        L".docx",
        L".pdf",
        L".lossless",
        L".q50",
        L".7zip",
        L".png",
        L".zip",
        L".jpg"
    };

    bool IsExtensionMonitored(const WCHAR* p_file_path);

    bool IsSameFileExtension(const WCHAR* p_file_path_1, const WCHAR* p_file_path_2);
}
#endif // DETECTOR_H