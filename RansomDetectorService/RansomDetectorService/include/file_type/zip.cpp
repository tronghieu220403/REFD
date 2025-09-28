#include "zip.h"
#include "../ulti/support.h"
#include <zlib.h>

namespace type_iden
{
    // Compute CRC32 with zlib
    uint32_t ComputeCRC32(const unsigned char* buf, size_t len) {
        return crc32(0L, buf, static_cast<uInt>(len));
    }

    // Inflate (zlib) compressed data
    bool InflateData(const unsigned char* comp, size_t comp_size,
        std::vector<unsigned char>& out, size_t expected_size) {
        out.resize(expected_size);
        z_stream zs{};
        zs.next_in = const_cast<Bytef*>(comp);
        zs.avail_in = static_cast<uInt>(comp_size);
        zs.next_out = out.data();
        zs.avail_out = static_cast<uInt>(expected_size);

        if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) return false;
        int ret = inflate(&zs, Z_FINISH);
        inflateEnd(&zs);

        return (ret == Z_STREAM_END && zs.total_out == expected_size);
    }

    // Dectect if a file is a ZIP-based file.
    // This method CAN NOT be trusted on encrypted ZIP file (a ZIP file with password)
    vector<string> GetZipTypes(const span<UCHAR>& data) {
        vector<string> types;

        if (data.size() < sizeof(EocdRecord)) {
            return types;  // Too small
        }

        // Search for EOCD from end (max comment = 64KB)
        const size_t max_back = 65536 + sizeof(EocdRecord);
        size_t search_start = (data.size() > max_back) ? data.size() - max_back : 0;

        size_t eocd_pos = string::npos;
        for (size_t i = data.size() - 4; i > search_start; --i) {
            if (data[i] == 0x50 && data[i + 1] == 0x4b &&
                data[i + 2] == 0x05 && data[i + 3] == 0x06) {
                eocd_pos = i;
                break;
            }
        }
        if (eocd_pos == string::npos) {
            return types;  // No EOCD
        }

        const EocdRecord* eocd = reinterpret_cast<const EocdRecord*>(&data[eocd_pos]);
        if (eocd->signature != 0x06054b50) {
            return types;
        }
        if (eocd->cd_offset + eocd->cd_size > data.size()) {
            return types;
        }

        // Scan central directory entries
        size_t pos = eocd->cd_offset;
        bool is_zip = true;
        bool has_docx = false, has_xlsx = false, has_pptx = false;

        for (int i = 0; i < eocd->total_records; i++) {
            if (pos + sizeof(CentralDirHeader) > data.size()) {
                is_zip = false;
                break;
            }
            const CentralDirHeader* cd = reinterpret_cast<const CentralDirHeader*>(&data[pos]);
            if (cd->signature != 0x02014b50) {
                is_zip = false;
                break;
            }

            size_t name_off = pos + sizeof(CentralDirHeader);
            size_t next_off = name_off + cd->name_len + cd->extra_len + cd->comment_len;

            if (next_off > data.size()) {
                is_zip = false;
                break;
            }

            string name(reinterpret_cast<const char*>(&data[name_off]), cd->name_len);
            ulti::ToLowerOverride(name);

            if (has_docx == false && name == "word/document.xml") has_docx = true;
            if (has_xlsx == false && name == "xl/workbook.xml") has_xlsx = true;
            if (has_pptx == false && name == "ppt/presentation.xml") has_pptx = true;

            if (cd->local_header_offset + sizeof(LocalFileHeader) > data.size())
            {
                is_zip = false;
                break;
            }

            pos = cd->local_header_offset;
            const LocalFileHeader* lh = reinterpret_cast<const LocalFileHeader*>(&data[pos]);
            if (lh == nullptr
                || lh->signature != 0x04034b50
                || (lh->comp_size != cd->comp_size || lh->uncomp_size != cd->uncomp_size)
                || lh->crc32 != cd->crc32)
            {
                is_zip = false;
                break;
            }
            
            size_t data_start = (size_t)pos + sizeof(LocalFileHeader) + lh->name_len + lh->extra_len;
            if (data_start + cd->comp_size > data.size())
            {
                is_zip = false;
                break;
            }

            const unsigned char* comp_ptr = &data[data_start];
            std::vector<unsigned char> decomp;

            if (cd->compression == 0) {
                // stored
                if (cd->uncomp_size != cd->comp_size) {
                    is_zip = false;
                    break;
                }

                uint32_t real_crc = ComputeCRC32(comp_ptr, cd->comp_size);
                if (real_crc != cd->crc32) {
                    is_zip = false;
                    break;
                }

            }
            else if (cd->compression == 8) {
                // deflate
                if (!InflateData(comp_ptr, cd->comp_size, decomp, cd->uncomp_size)) {
                    is_zip = false;
                    break;
                }
                uint32_t real_crc = ComputeCRC32(decomp.data(), decomp.size());
                if (real_crc != cd->crc32) {
                    is_zip = false;
                    break;
                }
            }
            else {
                nullptr;
            }

            pos = next_off;
        }

        if (is_zip)
        {
            types.push_back("zip");
        }

        if (has_docx) types.push_back("docx");
        if (has_xlsx) types.push_back("xlsx");
        if (has_pptx) types.push_back("pptx");

        return types;
    }

}
