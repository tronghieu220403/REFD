#include "txt.h"

namespace type_iden
{

	bool IsPrintableCodepoint(uint32_t cp) {
		if (cp == '\n' || cp == '\r' || cp == '\t') return true;
		if (cp >= 0x20 && cp < 0x7F) return true; // ASCII printable
		return iswprint((wint_t)cp) || iswspace((wint_t)cp);
	}

	bool CheckPrintableUTF16(const span<unsigned char>& buffer)
	{
		size_t i = 0;
		bool little_endian = true;

		// BOM check
		if (buffer[0] == 0xFF && buffer[1] == 0xFE) {
			little_endian = true;
			i = 2;
		}
		else if (buffer[0] == 0xFE && buffer[1] == 0xFF) {
			little_endian = false;
			i = 2;
		}

		streamsize printable_chars = 0;
		streamsize total_chars = buffer.size() / sizeof(wchar_t);

		auto read_u16 = [&](size_t idx) -> uint16_t {
			if (little_endian)
				return buffer[idx] | (buffer[idx + 1] << 8);
			else
				return (buffer[idx] << 8) | buffer[idx + 1];
			};

		while (i + 1 < buffer.size()) {
			uint16_t w1 = read_u16(i);
			i += 2;

			uint32_t codepoint = 0;

			if (w1 >= 0xD800 && w1 <= 0xDBFF) {
				// high surrogate
				if (i + 1 < buffer.size()) {
					uint16_t w2 = read_u16(i);
					if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
						codepoint = 0x10000 + (((w1 - 0xD800) << 10) | (w2 - 0xDC00));
						i += 2;
					}
					else {
						// invalid surrogate
						continue;
					}
				}
			}
			else {
				codepoint = w1;
			}

			total_chars++;
			if (IsPrintableCodepoint(codepoint)) {
				printable_chars++;
			}
		}

		if (total_chars == 0) {
			return false;
		}

		return !BelowTextThreshold(printable_chars, total_chars);
	}

	bool CheckPrintableUTF8(const span<unsigned char>& buffer)
	{
		streamsize printable_chars = 0;
		streamsize total_chars = 0;
		size_t i = 0;

		if (buffer.size() >= 3 && buffer[0] == 0xef && buffer[1] == 0xbb && buffer[2] == 0xbf) {
			i = 3; // skip UTF-8 BOM
		}
		while (i < buffer.size()) {
			unsigned char c = buffer[i];
			uint32_t codepoint = 0;
			size_t seq_len = 0;

			if (c < 0b10000000) { // 1-byte ASCII: 0xxxxxxx
				codepoint = c;
				seq_len = 1;
			}
			else if (i + 1 < buffer.size()
				&& (c & 0b11100000) == 0b11000000
				&& (buffer[i + 1] & 0b11000000) == 0b10000000)
				// 2-byte UTF-8: 110xxxxx 10xxxxxx
			{
				codepoint = ((c & 0b00011111) << 6) | (buffer[i + 1] & 0b00111111);
				seq_len = 2;
			}
			else if (i + 2 < buffer.size()
				&& (c & 0b11110000) == 0b11100000
				&& (buffer[i + 1] & 0b11000000) == 0b10000000
				&& (buffer[i + 2] & 0b11000000) == 0b10000000)
				// 3-byte UTF-8: 1110xxxx 10xxxxxx 10xxxxxx
			{
				codepoint = ((c & 0x0F) << 12) | ((buffer[i + 1] & 0b00111111) << 6) | (buffer[i + 2] & 0b00111111);
				seq_len = 3;
			}
			else if (i + 3 < buffer.size()
				&& (c & 0b11111000) == 0b11110000
				&& (buffer[i + 1] & 0b11000000) == 0b10000000
				&& (buffer[i + 2] & 0b11000000) == 0b10000000
				&& (buffer[i + 3] & 0b11000000) == 0b10000000)
				// 4-byte UTF-8: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
			{
				codepoint = ((c & 0b00000111) << 18) | ((buffer[i + 1] & 0b00111111) << 12)
					| ((buffer[i + 2] & 0b00111111) << 6) | (buffer[i + 3] & 0b00111111);
				seq_len = 4;
			}
			else {
				// invalid byte
				i++;
				continue;
			}

			total_chars++;
			if (IsPrintableCodepoint(codepoint)) {
				printable_chars++;
			}

			i += seq_len;
		}

		if (total_chars == 0) return false;
		return !BelowTextThreshold(printable_chars, total_chars);
	}

	bool CheckPrintableUTF32(const span<unsigned char>& buffer)
	{
		size_t i = 0;
		bool little_endian = true;

		// BOM check
		if (buffer.size() >= 4) {
			if (buffer[0] == 0xFF && buffer[1] == 0xFE &&
				buffer[2] == 0x00 && buffer[3] == 0x00) {
				little_endian = true;
				i = 4;
			}
			else if (buffer[0] == 0x00 && buffer[1] == 0x00 &&
				buffer[2] == 0xFE && buffer[3] == 0xFF) {
				little_endian = false;
				i = 4;
			}
		}

		auto read_u32 = [&](size_t idx) -> uint32_t {
			if (little_endian) {
				return (uint32_t)buffer[idx] |
					((uint32_t)buffer[idx + 1] << 8) |
					((uint32_t)buffer[idx + 2] << 16) |
					((uint32_t)buffer[idx + 3] << 24);
			}
			else {
				return ((uint32_t)buffer[idx] << 24) |
					((uint32_t)buffer[idx + 1] << 16) |
					((uint32_t)buffer[idx + 2] << 8) |
					(uint32_t)buffer[idx + 3];
			}
			};

		streamsize total_chars = 0;
		streamsize printable_chars = 0;

		while (i + 3 < buffer.size()) {
			uint32_t cp = read_u32(i);
			i += 4;

			// UTF-32 validity 
			if (cp > 0x10FFFF) continue;
			if (cp >= 0xD800 && cp <= 0xDFFF) continue; // invalid surrogate

			total_chars++;
			if (IsPrintableCodepoint(cp)) {
				printable_chars++;
			}
		}

		if (total_chars == 0) return false;

		return !BelowTextThreshold(printable_chars, total_chars);
	}

	vector<string> GetTxtTypes(const span<UCHAR>& data)
	{
		vector<string> ans;

		size_t sample_size = min<size_t>(1024, data.size());
		span<unsigned char> sample = data.first(sample_size);

		// Helper lambda: verify both sample and full file with one checkFn
		auto verify = [&](auto&& checkFn) -> bool {
			if (!checkFn(sample)) return false;
			if (sample_size <= 1024) return true;

			// Sample passed, now read full file
			return checkFn(data);
			};

		// Try encodings in order
		if (verify(CheckPrintableUTF8)
			|| verify(CheckPrintableUTF16)
			//|| verify(CheckPrintableUTF32)
			)
		{
			ans.push_back("txt");
		}

		return ans;
	}
}