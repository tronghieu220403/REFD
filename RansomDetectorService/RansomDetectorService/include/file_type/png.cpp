#include "png.h"
#include "../ulti/support.h"

namespace type_iden
{
    std::vector<std::string> GetPngTypes(const std::span<UCHAR>& data) {
        // PNG signature (8 bytes fixed)
        static const UCHAR kPngSig[8] = { 0x89, 'P','N','G', 0x0D,0x0A,0x1A,0x0A };
        std::vector<std::string> result;

        // File too small to be a valid PNG
        if (data.size() < 8) 
            return result;

        // Check signature
        if (memcmp(data.data(), kPngSig, 8) != 0) 
            return result;

        size_t offset = 8;
        bool seenIHDR = false;
        bool seenIEND = false;

        // Parse all chunks until IEND or EOF
        while (offset + 12 <= data.size()) {
            // Read chunk length (big-endian)
            uint32_t length = (data[offset] << 24) | (data[offset + 1] << 16) |
                (data[offset + 2] << 8) | (data[offset + 3]);
            offset += 4;

            // Read chunk type (4 ASCII chars)
            if (offset + 4 > data.size()) 
                return result;
            const UCHAR* typePtr = &data[offset];
            offset += 4;

            // Check bounds for Data + CRC
            if (offset + length + 4 > data.size()) 
                return result;
            const UCHAR* dataPtr = &data[offset];
            offset += length;

            // Read CRC stored in file
            uint32_t crcRead = (data[offset] << 24) | (data[offset + 1] << 16) |
                (data[offset + 2] << 8) | (data[offset + 3]);
            offset += 4;

            // Recalculate CRC (on Type + Data)
            uLong crcCalc = crc32(0L, Z_NULL, 0);
            crcCalc = crc32(crcCalc, typePtr, 4);
            crcCalc = crc32(crcCalc, dataPtr, length);

            // If CRC mismatch -> corrupted file
            if (crcCalc != crcRead) 
                return result;

            // Track important chunks
            if (memcmp(typePtr, "IHDR", 4) == 0) seenIHDR = true;
            if (memcmp(typePtr, "IEND", 4) == 0) {
                seenIEND = true;
                break; // stop parsing
            }
        }

        // If we saw both IHDR and IEND with valid CRCs, it's a PNG
        if (seenIHDR && seenIEND) {
            result.push_back("png");
        }
        return result;
    }

}