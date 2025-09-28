#include "png.h"
#include "../ulti/support.h"
#include <jpeglib.h>

namespace type_iden
{
    using UCHAR = unsigned char;

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

}