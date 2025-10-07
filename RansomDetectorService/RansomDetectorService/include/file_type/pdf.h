#ifndef FILE_TYPE_PDF_H_
#define FILE_TYPE_PDF_H_

#include "../ulti/include.h"

namespace type_iden {

    // Detect if a memory buffer contains a valid PDF file.
    // Returns {"pdf"} if valid, empty vector if corrupted.
    std::vector<std::string> GetPdfTypes(const std::span<unsigned char>& data);

}  // namespace type_iden

#endif  // FILE_TYPE_PDF_H_
