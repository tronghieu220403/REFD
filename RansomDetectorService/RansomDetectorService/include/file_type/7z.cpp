#pragma once
#include "7z.h"
#include <archive.h>

namespace type_iden
{
    // Return {"7z"} if the archive is valid, otherwise return an empty vector
    std::vector<std::string> Get7zTypes(const std::span<UCHAR>& data) {
        std::vector<std::string> result;

        struct archive* a = archive_read_new();
        if (!a) {
            return result;
        }

        // Ensure cleanup on exit
        defer{ archive_read_free(a); };

        archive_read_support_format_7zip(a);
        archive_read_support_filter_all(a);

        if (archive_read_open_memory(a, data.data(), data.size()) != ARCHIVE_OK) {
            return result;
        }

        struct archive_entry* entry;
        bool ok = true;

        // Iterate through all entries in the archive
        while (true) {
            int r = archive_read_next_header(a, &entry);
            if (r == ARCHIVE_EOF) break;   // End of archive
            if (r != ARCHIVE_OK) {
                ok = false;
                break;
            }

            // Skip full entry data to force CRC validation
            // Do not use archive_read_data_skip
            UCHAR buf[8192];
            while (true) {
                auto size = archive_read_data(a, buf, sizeof(buf));
                if (size == 0) break;      // End of entry
                if (size < 0) {
                    ok = false;
                    break;
                }
            }
            if (!ok) break;
        }

        if (ok) {
            result.push_back("7z");
        }
        return result;
    }
}