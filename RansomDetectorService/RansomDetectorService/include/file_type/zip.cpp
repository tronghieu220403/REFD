#include "zip.h"
#include "../ulti/support.h"
#include <archive.h>
#include <archive_entry.h>

namespace type_iden
{

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

    // Dectect if a file is a ZIP-based file.
    vector<string> GetZipTypes(const span<UCHAR>& data)
    {
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

            // Data is encrypted
            if (cd->flags & 0b1) { is_zip = false; break; }


            pos = cd->local_header_offset;
            if (pos + sizeof(LocalFileHeader) > data.size()) { is_zip = false; break; }
            const LocalFileHeader* lh = reinterpret_cast<const LocalFileHeader*>(&data[pos]);
            if (lh == nullptr || lh->signature != 0x04034b50)
            {
                is_zip = false;
                break;
            }

            size_t data_start = (size_t)pos + sizeof(LocalFileHeader) + lh->name_len + lh->extra_len;
            uint32_t crc32_val = lh->crc32;
            uint32_t comp_size = lh->comp_size;
            uint32_t uncomp_size = lh->uncomp_size;
            if (data_start + comp_size > data.size()) { is_zip = false; break; }
            // If Data Descriptor bit is set (bit 3), local header sizes/CRC is zero, the correct values are put in the data descriptor immediately following the compressed data
            if (cd->flags & 0b1000)
            {
                size_t after_data = data_start + cd->comp_size;
                if (!ReadDataDescriptor(data.data(), data.size(), after_data, crc32_val, comp_size, uncomp_size)) {
                    is_zip = false; break;
                }
            }

            if ((comp_size != cd->comp_size || uncomp_size != cd->uncomp_size) || crc32_val != cd->crc32)
            {
                is_zip = false;
                break;
            }
        }

        if (is_zip == false)
        {
            return types;
        }

        struct archive* a = archive_read_new();
        archive_read_support_format_zip(a); // enable ZIP format

        // Ensure cleanup on exit
        defer{ archive_read_free(a); };

        // Open from memory buffer
        if (archive_read_open_memory(a, (void*)data.data(), data.size()) != ARCHIVE_OK) {
            return types; // invalid or corrupted
        }
        defer{ archive_read_close(a); };

        bool has_docx = false, has_xlsx = false, has_pptx = false;

        bool has_epub_mimetype = false, has_epub_container = false;
        string epub_mimetype_content;

        bool has_ods_content = false, has_ods_manifest = false;
        bool has_oxps_content_types = false, has_oxps_rels = false;

        struct archive_entry* entry;
        while (true) {
            int r = archive_read_next_header(a, &entry);
            if (r == ARCHIVE_EOF) {
                break; // finished reading all entries
            }
            if (r != ARCHIVE_OK) {
                return types; // corrupted
            }

            string name(archive_entry_pathname(entry));
            ulti::ToLowerOverride(name);

            if (has_docx == false && name == "word/document.xml") has_docx = true;
            if (has_xlsx == false && name == "xl/workbook.xml") has_xlsx = true;
            if (has_pptx == false && name == "ppt/presentation.xml") has_pptx = true;
            else if (has_epub_mimetype == false && name == "mimetype") {
                has_epub_mimetype = true;
                // Read its content to check value
                char buf[256] = {};
                la_ssize_t n = archive_read_data(a, buf, sizeof(buf) - 1);
                if (n > 0) {
                    epub_mimetype_content.assign(buf, buf + n);
                    // trim
                    while (!epub_mimetype_content.empty() &&
                        (epub_mimetype_content.back() == '\n' || epub_mimetype_content.back() == '\r' || epub_mimetype_content.back() == ' '))
                    {
                        epub_mimetype_content.pop_back();
                    }
                }
            }
            else if (has_epub_container == false && name == "meta-inf/container.xml") {
                has_epub_container = true;
            }
            else if (has_ods_content == false && name == "content.xml") {
                has_ods_content = true;
            }
            else if (has_ods_manifest == false && name == "meta-inf/manifest.xml") {
                has_ods_manifest = true;
            }
            else if (has_oxps_content_types == false && name == "[content_types].xml") {
                has_oxps_content_types = true;
            }
            else if (has_oxps_rels == false && name == "_rels/.rels") {
                has_oxps_rels = true;
            }

            // Read file data (this validates CRC)
            char buf[8192];
            while (true) {
                la_ssize_t r = archive_read_data(a, buf, sizeof(buf));
                if (r < 0) {
                    return types; // corrupted
                }
                if (r == 0) break; // EOF for this entry
            }
        }

        // If we reach here -> ZIP valid
        types.push_back("zip");

        if (has_docx) types.push_back("docx");
        if (has_xlsx) types.push_back("xlsx");
        if (has_pptx) types.push_back("pptx");

        if (has_epub_mimetype 
            && has_epub_container 
            && epub_mimetype_content == "application/epub+zip")
        {
            types.push_back("epub");
        }

        if (has_ods_content && has_ods_manifest) {
            types.push_back("ods");
        }

        if (has_oxps_content_types && has_oxps_rels) {
            types.push_back("oxps");
        }

        return types;
    }
}
