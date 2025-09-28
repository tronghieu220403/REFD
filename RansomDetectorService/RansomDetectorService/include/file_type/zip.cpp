#include "zip.h"
#include "../ulti/support.h"
#include <zlib.h>
#include <lzma.h>
#include <bzlib.h>

namespace type_iden
{
    // Compute CRC32 with zlib
    uint32_t ComputeCRC32(const unsigned char* buf, size_t len) {
        return crc32(0L, buf, static_cast<uInt>(len));
    }

    bool ReadDataDescriptor(const unsigned char* base, size_t file_size, size_t after_data_offset,
        uint32_t& crc32_out, uint32_t& comp_size_out, uint32_t& uncomp_size_out)
    {
        if (after_data_offset + 12 > file_size) {
            return false;
        }

        const uint32_t sig = *reinterpret_cast<const uint32_t*>(base + after_data_offset);
        if (sig == 0x08074b50) {
            if (after_data_offset + 16 > file_size) return false;
            const DataDescriptor* dd = reinterpret_cast<const DataDescriptor*>(base + after_data_offset);
            crc32_out = dd->crc32;
            comp_size_out = dd->comp_size;
            uncomp_size_out = dd->uncomp_size;
            return true;
        }
        else { // No signature
            const uint32_t* p = reinterpret_cast<const uint32_t*>(base + after_data_offset);
            crc32_out = p[0];
            comp_size_out = p[1];
            uncomp_size_out = p[2];
            return true;
        }
    }

    bool DecompressStored(const unsigned char* comp, size_t comp_size,
        std::vector<unsigned char>& out, size_t expected_size) {
        if (comp_size != expected_size) return false;
        out.assign(comp, comp + comp_size);
        return true;
    }

    // Inflate (zlib) compressed data
    bool DecompressDeflate(const unsigned char* comp, size_t comp_size,
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

    bool DecompressBzip2(const unsigned char* comp, size_t comp_size,
        std::vector<unsigned char>& out, size_t expected_size) {
        out.resize(expected_size);
        unsigned int destLen = static_cast<unsigned int>(expected_size);

        int ret = BZ2_bzBuffToBuffDecompress(
            reinterpret_cast<char*>(out.data()), &destLen,
            reinterpret_cast<char*>(const_cast<unsigned char*>(comp)),
            static_cast<unsigned int>(comp_size),
            0, 0);

        return (ret == BZ_OK && destLen == expected_size);
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
            if (*reinterpret_cast<const uint32_t*>(&data[i]) == 0x06054b50) {
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
            if (pos + sizeof(CentralDirHeader) > data.size()) { is_zip = false; break; }
            const CentralDirHeader* cd = reinterpret_cast<const CentralDirHeader*>(&data[pos]);
            if (cd->signature != 0x02014b50) { is_zip = false; break; }

            size_t name_off = pos + sizeof(CentralDirHeader);
            const size_t next_off = name_off + cd->name_len + cd->extra_len + cd->comment_len;

            if (next_off > data.size()) { is_zip = false; break; }
            defer{
                pos = next_off;
            };

            string name(reinterpret_cast<const char*>(&data[name_off]), cd->name_len);
            ulti::ToLowerOverride(name);

            if (has_docx == false && name == "word/document.xml") has_docx = true;
            if (has_xlsx == false && name == "xl/workbook.xml") has_xlsx = true;
            if (has_pptx == false && name == "ppt/presentation.xml") has_pptx = true;

            pos = cd->local_header_offset;
            if (pos + sizeof(LocalFileHeader) > data.size()) { is_zip = false; break; }

            const LocalFileHeader* lh = reinterpret_cast<const LocalFileHeader*>(&data[pos]);
            if (lh == nullptr
                || lh->signature != 0x04034b50
                || (lh->comp_size != cd->comp_size || lh->uncomp_size != cd->uncomp_size)
                || lh->crc32 != cd->crc32)
            {
                is_zip = false;
                break;
            }

            if (cd->flags & 0b1) // Data is encrypted
            {
                continue;
            }

            size_t data_start = (size_t)pos + sizeof(LocalFileHeader) + lh->name_len + lh->extra_len;
            if (data_start + cd->comp_size > data.size()) { is_zip = false; break; }

            uint32_t crc32_val = cd->crc32;
            uint32_t comp_size = cd->comp_size;
            uint32_t uncomp_size = cd->uncomp_size;

            // If Data Descriptor bit is set (bit 3), local header sizes/CRC is zero, the correct values are put in the data descriptor immediately following the compressed data
            if (cd->flags & 0b1000)
            {
                size_t after_data = data_start + comp_size;
                if (!ReadDataDescriptor(data.data(), data.size(), after_data, crc32_val, comp_size, uncomp_size)) {
                    is_zip = false; break;
                }
            }

            const unsigned char* comp_ptr = &data[data_start];
            std::vector<unsigned char> decomp;

            bool ok = true;
            if (cd->compression == 0) { // no compression
                if (uncomp_size != comp_size) { is_zip = false; break; }
            }
            else if (cd->compression == 8) {
                ok = DecompressDeflate(comp_ptr, comp_size, decomp, uncomp_size);
            }
            else if (cd->compression == 12) {
                ok = DecompressBzip2(comp_ptr, comp_size, decomp, uncomp_size);
            }
            else {
                // Not supported
                is_zip = false; break;
            }
            if (ok == false) { is_zip = false; break; }
             uint32_t real_crc = (cd->compression == 0) ? ComputeCRC32(comp_ptr, comp_size) : ComputeCRC32(decomp.data(), decomp.size());
            if (real_crc != crc32_val) { is_zip = false; break; }
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
