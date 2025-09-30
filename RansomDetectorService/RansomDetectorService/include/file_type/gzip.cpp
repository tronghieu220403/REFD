#include "gzip.h"
#include <archive.h>
#include <archive_entry.h>

namespace type_iden {

    // Return {"gzip"} if buffer is valid gzip (validated by libarchive).
    std::vector<std::string> GetGzipTypes(const std::span<UCHAR>& data) {
        std::vector<std::string> types;

        struct archive* a = archive_read_new();
        if (!a) return types;

        // Ensure cleanup on exit
        defer{ archive_read_free(a); };

        // Enable gzip support explicitly.
        archive_read_support_filter_gzip(a);
        // Also support raw to avoid "unknown format".
        archive_read_support_format_tar(a);

        // Open from memory buffer.
        if (archive_read_open_memory(a, data.data(), data.size()) != ARCHIVE_OK) {
            return types;
        }
        defer{ archive_read_close(a); };

        struct archive_entry* entry;
        bool ok = true;

        int r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_OK) {
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
        }

        if (ok == true)
        {
            types.push_back("gzip");
        }

        return types;
    }

}  // namespace type_iden
