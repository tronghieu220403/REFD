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

    bool DecompressLzma(const unsigned char* comp, size_t comp_size,
        std::vector<unsigned char>& out, size_t expected_size) {
        out.resize(expected_size);
        lzma_stream strm = LZMA_STREAM_INIT;

        if (lzma_auto_decoder(&strm, UINT64_MAX, 0) != LZMA_OK) {
            return false;
        }

        strm.next_in = comp;
        strm.avail_in = comp_size;
        strm.next_out = out.data();
        strm.avail_out = expected_size;

        lzma_ret ret = lzma_code(&strm, LZMA_FINISH);
        size_t total = strm.total_out;
        lzma_end(&strm);

        return (ret == LZMA_STREAM_END && total == expected_size);
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
            const size_t next_off = name_off + cd->name_len + cd->extra_len + cd->comment_len;

            if (next_off > data.size()) {
                is_zip = false;
                break;
            }
            defer{
                pos = next_off;
            };

            string name(reinterpret_cast<const char*>(&data[name_off]), cd->name_len);
            ulti::ToLowerOverride(name);

            if (has_docx == false && name == "word/document.xml") has_docx = true;
            if (has_xlsx == false && name == "xl/workbook.xml") has_xlsx = true;
            if (has_pptx == false && name == "ppt/presentation.xml") has_pptx = true;

            pos = cd->local_header_offset;
            if (pos + sizeof(LocalFileHeader) > data.size())
            {
                is_zip = false;
                break;
            }
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
            if (cd->flags & 0x0001) // encrypted
            {
                continue;
            }

            bool ok = true;
            if (cd->compression == 0) { // no compression
                if (cd->uncomp_size != cd->comp_size) {
                    is_zip = false;
                    break;
                }
            }
            if (cd->compression == 8) {
                ok = DecompressDeflate(comp_ptr, cd->comp_size, decomp, cd->uncomp_size);
            }
            else if (cd->compression == 12) {
                ok = DecompressBzip2(comp_ptr, cd->comp_size, decomp, cd->uncomp_size);
            }
            else if (cd->compression == 14) {
                //ok = DecompressLzma(comp_ptr, cd->comp_size, decomp, cd->uncomp_size);
                continue;
            }
            else if (cd->compression == 98) {
                //ok = DecompressPpmd(comp_ptr, cd->comp_size, decomp, cd->uncomp_size);
                continue;
            }
            if (ok == false)
            {
                is_zip = false;
                break;
            }
             uint32_t real_crc = (cd->compression == 0) ? ComputeCRC32(comp_ptr, cd->comp_size) : ComputeCRC32(decomp.data(), decomp.size());
            if (real_crc != cd->crc32) {
                is_zip = false;
                break;
            }
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
