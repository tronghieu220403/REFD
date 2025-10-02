#include "gzip.h"
#include <archive.h>
#include <archive_entry.h>

namespace type_iden {

    // Compute Adler-32 for a buffer
    static uint32_t Adler32(const unsigned char* data, size_t size) {
        return adler32(1L, data, static_cast<uInt>(size));
    }

    std::vector<std::string> GetZlibTypes(const std::span<UCHAR>& data) {
        std::vector<std::string> types;

        if (data.size() < 6) {
            // too small: must have header + checksum
            return types;
        }

        // Prepare z_stream
        z_stream strm{};
        strm.next_in = const_cast<Bytef*>(data.data());
        strm.avail_in = static_cast<uInt>(data.size());

        if (inflateInit(&strm) != Z_OK) {
            return types;
        }

        std::vector<unsigned char> out;
        out.reserve(8192);

        int ret = Z_OK;
        UCHAR buf[8192];

        // Decompress loop
        do {
            strm.next_out = buf;
            strm.avail_out = sizeof(buf);

            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret == Z_DATA_ERROR || ret == Z_MEM_ERROR || ret == Z_NEED_DICT) {
                inflateEnd(&strm);
                return types;  // corrupted
            }

            size_t have = sizeof(buf) - strm.avail_out;
            out.insert(out.end(), buf, buf + have);

        } while (ret != Z_STREAM_END);

        inflateEnd(&strm);

        // If stream not ended correctly -> corrupted
        if (ret != Z_STREAM_END) {
            return types;
        }

        // Verify Adler-32 checksum at end of input
        if (data.size() < 4) {
            return types;
        }

        // Last 4 bytes in network byte order
        size_t adler_offset = data.size() - 4;
        uint32_t stored = (data[adler_offset] << 24) |
            (data[adler_offset + 1] << 16) |
            (data[adler_offset + 2] << 8) |
            (data[adler_offset + 3]);

        uint32_t computed = Adler32(out.data(), out.size());
        if (stored != computed) {
            return types;  // checksum mismatch
        }

        types.push_back("zlib");
        return types;
    }


}  // namespace type_iden
