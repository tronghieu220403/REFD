#include "file_type_iden.h"

namespace type_iden
{
	bool CheckPrintableUTF16(const std::vector<unsigned char>& buffer)
	{
		if (buffer.size() == 0) {
			return true;
		}

		std::streamsize printable_chars = 0;
		std::streamsize total_chars = buffer.size() / sizeof(wchar_t);

		for (size_t i = 0; i < buffer.size(); i += 2)
		{
			wchar_t c = *(wchar_t*)&buffer[i];
			if (iswprint(c) || iswspace(c)) {
				printable_chars++;
			}
		}

		if (total_chars == 0) {
			return false;
		}

		return !BelowTextThreshold(printable_chars, total_chars);
	}

	bool CheckPrintableUTF8(const std::vector<unsigned char>& buffer)
	{
		if (buffer.size() == 0) {
			return true;
		}

		std::streamsize printable_chars = 0;
		std::streamsize total_chars = 0;
		size_t i = 0;
		while (i < buffer.size()) {

			unsigned char c = buffer[i];
			if (c < 0x80) { // 1-byte ASCII (7-bit)
				total_chars++;
				if (isprint(c) || isspace(c)) {
					printable_chars++;
				}
				i++;
			}
			else if ((c & 0xE0) == 0xC0) { // 2-byte UTF-8
				if (i + 1 < buffer.size()) {
					wchar_t wchar = ((c & 0x1F) << 6) | (buffer[i + 1] & 0x3F);
					total_chars++;
					if (iswprint(wchar) || iswspace(wchar)) {
						printable_chars++;
					}
				}
				i += 2;
				/*
				// Should be like this:
				if (i + 1 < buffer.size()) {
					if (buffer.size() == BEGIN_WIDTH + BEGIN_WIDTH && i < BEGIN_WIDTH && i + 1 >= BEGIN_WIDTH) // Char between the merge may not correct
					{
						i = BEGIN_WIDTH;
						continue;
					}
					wchar_t wchar = ((c & 0x1F) << 6) | (buffer[i + 1] & 0x3F);
					if (iswprint(wchar) || iswspace(wchar)) {
						total_chars += 2;
						printable_chars += 2;
						i += 2;
					}
					else
					{
						if (buffer.size() == BEGIN_WIDTH + BEGIN_WIDTH && i >= BEGIN_WIDTH && i <= BEGIN_WIDTH + 2) // May be there is a first char in UTF-8 char disappeared.
						{
							i += 1;
						}
						else
						{
							total_chars += 2;
							i += 2;
						}
					}
				}
				*/
			}
			else if ((c & 0xF0) == 0xE0) { // 3-byte UTF-8
				if (i + 2 < buffer.size()) {
					wchar_t wchar = ((c & 0x0F) << 12) | ((buffer[i + 1] & 0x3F) << 6) | (buffer[i + 2] & 0x3F);
					total_chars++;
					if (iswprint(wchar) || iswspace(wchar)) {
						printable_chars++;
					}
				}
				i += 3;
			}
			else if ((c & 0xF8) == 0xF0) { // 4-byte UTF-8
				if (i + 3 < buffer.size()) {
					wchar_t wchar = ((c & 0x07) << 18) | ((buffer[i + 1] & 0x3F) << 12) | ((buffer[i + 2] & 0x3F) << 6) | (buffer[i + 3] & 0x3F);
					total_chars++;
					if (iswprint(wchar) || iswspace(wchar)) {
						printable_chars++;
					}
				}
				i += 4;
			}
			else {
				i++; // Skip invalid bytes
			}
		}

		if (total_chars == 0) {
			return false;
		}

		return !BelowTextThreshold(printable_chars, total_chars);
	}

	bool CheckPrintableANSI(const std::vector<unsigned char>& buffer)
	{
		if (buffer.size() == 0) {
			return true;
		}

		std::streamsize printable_chars = 0;
		std::streamsize total_chars = 0;

		for (unsigned char c : buffer) {
			total_chars++;
			if (isprint(c) || isspace(c)) {
				printable_chars++;
			}
		}

		if (total_chars == 0) {
			return false;
		}

		return !BelowTextThreshold(printable_chars, total_chars);
	}
}
