#pragma once
#include "../ulti/include.h"

namespace type_iden
{
#pragma pack(push, 1)
    struct EocdRecord {
        uint32_t signature;       // 0x06054b50
        uint16_t disk_number;
        uint16_t cd_start_disk;
        uint16_t records_on_disk;
        uint16_t total_records;
        uint32_t cd_size;
        uint32_t cd_offset;
        uint16_t comment_length;
        // comment (n bytes) follows
    };

    struct CentralDirHeader {
        uint32_t signature;       // 0x02014b50
        uint16_t version_made;
        uint16_t version_needed;
        uint16_t flags;
        uint16_t compression;
        uint16_t mod_time;
        uint16_t mod_date;
        uint32_t crc32;
        uint32_t comp_size;
        uint32_t uncomp_size;
        uint16_t name_len;
        uint16_t extra_len;
        uint16_t comment_len;
        uint16_t disk_start;
        uint16_t int_attr;
        uint32_t ext_attr;
        uint32_t local_header_offset;
        // name + extra + comment follows
    };

    struct LocalFileHeader {
        uint32_t signature;   // 0x04034b50
        uint16_t version_needed;
        uint16_t flags;
        uint16_t compression;
        uint16_t mod_time;
        uint16_t mod_date;
        uint32_t crc32;
        uint32_t comp_size;
        uint32_t uncomp_size;
        uint16_t name_len;
        uint16_t extra_len;
        // name + extra follows
    };
#pragma pack(pop)

	vector<string> GetZipTypes(const span<UCHAR>& data);
}