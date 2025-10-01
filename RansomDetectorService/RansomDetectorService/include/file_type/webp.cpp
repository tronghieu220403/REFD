#include "webp.h"
#include <webp/decode.h>

namespace type_iden {

    // Helper to read little-endian 32-bit
    static uint32_t ReadLE32(const unsigned char* p) {
        return static_cast<uint32_t>(p[0]) |
            (static_cast<uint32_t>(p[1]) << 8) |
            (static_cast<uint32_t>(p[2]) << 16) |
            (static_cast<uint32_t>(p[3]) << 24);
    }

    // Fully validate WebP file by parsing chunks and decoding with libwebp
    std::vector<std::string> GetWebpTypes(const std::span<UCHAR>& data) {
        std::vector<std::string> types;

        if (data.size() < 12) return types;

        // RIFF + WEBP header check
        if (std::memcmp(data.data(), "RIFF", 4) != 0) return types;
        if (std::memcmp(data.data() + 8, "WEBP", 4) != 0) return types;

        size_t file_size = data.size();
        uint32_t riff_size = ReadLE32(data.data() + 4);
        if (riff_size + 8 != file_size) return types;

        WebPBitstreamFeatures features;
            
        VP8StatusCode st = WebPGetFeatures(data.data(), data.size(), &features);
        if (st != VP8_STATUS_OK) {
            return types;  // corrupt bitstream
        }

        // Attempt to decode into a temporary buffer
        int w = features.width;
        int h = features.height;
        if (w <= 0 || h <= 0) return types;
        size_t buf_size = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
        static std::vector<uint8_t> rgba;
        rgba.reserve(buf_size);
        rgba.resize(buf_size);

        uint8_t* out = WebPDecodeRGBAInto(data.data(), data.size(),
            rgba.data(), buf_size,
            w * 4);
        if (!out) {
            return types;  // decode failed
        }

        types.emplace_back("webp");
        return types;
    }
}  // namespace type_iden
