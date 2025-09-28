#include "png.h"
#include "../ulti/support.h"

namespace type_iden
{
    std::vector<std::string> GetJpgTypes(const std::span<UCHAR>& data) {
        std::vector<std::string> result;

        // Minimal size check: SOI (2 bytes) + EOI (2 bytes)
        if (data.size() < 4) {
            return result;
        }

        // Check SOI marker
        if (!(data[0] == 0xFF && data[1] == 0xD8)) {
            return result;
        }

        // Check EOI marker
        if (!(data[data.size() - 2] == 0xFF && data[data.size() - 1] == 0xD9)) {
            return result;
        }

        // Walk through segments
        size_t pos = 2; // start after SOI
        while (pos + 4 <= data.size()) {
            if (data[pos] != 0xFF) {
                // Invalid marker
                return result;
            }

            uint8_t marker = data[pos + 1];
            pos += 2;

            // Standalone markers (no length field)
            if (marker == 0xD0 || marker == 0xD1 || marker == 0xD2 || marker == 0xD3 ||
                marker == 0xD4 || marker == 0xD5 || marker == 0xD6 || marker == 0xD7 ||
                marker == 0xD8 || marker == 0xD9) {
                continue;
            }

            // Length must be at least 2 (for the length field itself)
            if (pos + 2 > data.size()) {
                return result;
            }
            uint16_t seg_len = (data[pos] << 8) | data[pos + 1];
            if (seg_len < 2) {
                return result;
            }

            pos += seg_len; // skip this segment

            if (pos > data.size()) {
                // Out of range => corrupt
                return result;
            }

            if (marker == 0xDA) {
                // SOS (Start of Scan): image data until EOI
                break;
            }
        }

        // If we reached here without errors, mark as jpeg
        result.push_back("jpg");
        return result;
    }


}