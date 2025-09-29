#ifndef TYPE_IDEN_OLE_H_
#define TYPE_IDEN_OLE_H_

#include "../ulti/include.h"

namespace type_iden {

    // DeepValidateOle: validate OLE file by walking storages/streams.
    // Returns true if valid, false if corrupt. 
    bool DeepValidateOle(const std::span<UCHAR>& data);

    // GetOleTypes: return {"ole"} if file is a valid OLE Compound Document
    // (signature + COM open + deep validation with strict mode).
    std::vector<std::string> GetOleTypes(const std::span<UCHAR>& data);

}  // namespace type_iden

#endif  // TYPE_IDEN_OLE_H_
