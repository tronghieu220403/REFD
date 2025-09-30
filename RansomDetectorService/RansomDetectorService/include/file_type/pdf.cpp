#include "pdf.h"

#include <qpdf/QPDF.hh>

namespace type_iden {

    std::vector<std::string> GetPdfTypes(const std::span<UCHAR>& data) {
        std::vector<std::string> types;

        if (data.size() < sizeof("%PDF-") - 1) return{};
        if (std::memcmp(data.data(), "%PDF-", sizeof("%PDF-") - 1) != 0)
        {
            return {};
        }

        int corrupt_count = 0;
        int total_objects = 0;
        int stream_count = 0;
        try {
            QPDF pdf;
            pdf.setAttemptRecovery(true);
            pdf.setSuppressWarnings(true);

            // Parse directly from memory buffer
            pdf.processMemoryFile("buffer", reinterpret_cast<const char*>(data.data()), data.size());

            // Root catalog must exist
            if (!pdf.getRoot().isDictionary()) return {};

            total_objects = pdf.getObjectCount();
            for (int objid = 1; objid <= total_objects; ++objid)
            {
                try {
                    QPDFObjectHandle obj = pdf.getObjectByID(objid, 0);
                }
                catch (const QPDFExc& e) {
                    return {};
                }
            }
        }
        catch (const QPDFExc& e) {
            return {};
        }
        catch (const std::exception&) {
            return {};
        }
        types.emplace_back("pdf");

        return types;
    }

}  // namespace type_iden
