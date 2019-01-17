#ifndef CLR_PROFILER_CLRHELPER_H_
#define CLR_PROFILER_CLRHELPER_H_

#include "string.h"
#include <vector>
#include "util.h"

namespace trace {

    enum MethodArgumentFlag
    {
        MethodArgumentFlag_None = 0,
        MethodArgumentFlag_VOID = 1,
        MethodArgumentFlag_BOX = 2
    };

    struct MethodArgument
    {
        INT64 offset;
        INT64 length;
        PCCOR_SIGNATURE pbBase;
        MethodArgumentFlag GetFlags() const;
    };

    struct MethodSignature {
    private:
        PCCOR_SIGNATURE pbBase;
        unsigned len;
        
        ULONG numberOfTypeArguments = 0;
        ULONG numberOfArguments = 0;
        
        MethodArgument ret{};
        std::vector<MethodArgument> params;

    public:

        MethodSignature(PCCOR_SIGNATURE pb, const int cbBuffer)
        {
            pbBase = pb;
            len = cbBuffer;
        }

        HRESULT TryParse();

        bool operator ==(const MethodSignature& other) const {
            return memcmp(pbBase, other.pbBase, len);
        }

        CorCallingConvention CallingConvention() const {
            return CorCallingConvention(len == 0 ? 0 : pbBase[0]);
        }

        ULONG NumberOfTypeArguments() const {
            return numberOfTypeArguments;
        }

        ULONG NumberOfArguments() const {
            return numberOfArguments;
        }

        WSTRING str() const {
            return HexStr(pbBase, len);
        }

        bool IsVoidMethod() const {
            return ret.GetFlags() == MethodArgumentFlag_VOID;
        }

        std::vector<MethodArgument> GetMethodArguments() const {
            return params;
        }
    };

}

#endif  // CLR_PROFILER_CLRHELPER_H_
