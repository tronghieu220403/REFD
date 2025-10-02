#include "image.h"
#include "../ulti/support.h"
#include <jpeglib.h>
#include <webp/decode.h>

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

    // Custom error handler to avoid exit() inside libjpeg
    struct JpegErrorManager {
        jpeg_error_mgr pub;
        jmp_buf setjmp_buffer;
    };

    extern "C" {
        static void JpegErrorExit(j_common_ptr cinfo) {
            JpegErrorManager* err = (JpegErrorManager*)cinfo->err;
            longjmp(err->setjmp_buffer, 1);
        }

        // Custom output_message (no-op, disable stderr logs)
        static void JpegOutputMessage(j_common_ptr cinfo) {
            // Do nothing (suppress all libjpeg messages)
            return;
        }
    }

    std::vector<std::string> GetJpgTypes(const std::span<UCHAR>& data) {
        std::vector<std::string> result;

        if (data.size() < 4) return result;
        if (!(data[0] == 0xFF && data[1] == 0xD8)) return result;
        if (!(data[data.size() - 2] == 0xFF && data[data.size() - 1] == 0xD9)) return result;

        // libjpeg structures
        jpeg_decompress_struct cinfo{};
        JpegErrorManager jerr{};

        cinfo.err = jpeg_std_error(&jerr.pub);
        jerr.pub.error_exit = JpegErrorExit;
        jerr.pub.output_message = JpegOutputMessage;

        if (setjmp(jerr.setjmp_buffer)) {
            // libjpeg reported error -> corrupt JPEG
            jpeg_destroy_decompress(&cinfo);
            return result;
        }

        jpeg_create_decompress(&cinfo);

        // Feed buffer
        jpeg_mem_src(&cinfo, (const unsigned char*)data.data(), data.size());

        // Try reading header
        if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
            jpeg_destroy_decompress(&cinfo);
            return result;
        }

        // Try to start decompression (entropy check happens here)
        jpeg_start_decompress(&cinfo);

        // Read scanlines to fully validate entropy data
        const int row_stride = cinfo.output_width * cinfo.output_components;
        std::vector<unsigned char> buffer(row_stride);
        while (cinfo.output_scanline < cinfo.output_height) {
            unsigned char* row_ptr = buffer.data();
            jpeg_read_scanlines(&cinfo, &row_ptr, 1);
        }

        // Clean up
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

        // If no errors -> valid JPEG
        result.push_back("jpg");
        return result;
    }

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
}