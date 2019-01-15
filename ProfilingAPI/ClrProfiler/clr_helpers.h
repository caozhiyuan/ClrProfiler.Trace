#ifndef CLR_PROFILER_CLRHELPER_H_
#define CLR_PROFILER_CLRHELPER_H_

#include <iomanip>
#include <vector>
#include "string.h"

namespace trace {

    constexpr char HexMap[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

    std::wstring HexStr(const unsigned char *data, int len)
    {
        std::wstring s(len * 2, ' ');
        for (int i = 0; i < len; ++i) {
            s[2 * i] = HexMap[(data[i] & 0xF0) >> 4];
            s[2 * i + 1] = HexMap[data[i] & 0x0F];
        }
        return s;
    }

    struct MethodSignature {
    public:
        const unsigned char * data;
        int len = 0;

        MethodSignature(const unsigned char * data, const int len) : data(data),len(len) {}

        inline bool operator==(const MethodSignature& other) const {
            return memcmp(data, other.data, len);
        }

        CorCallingConvention CallingConvention() const {
            return CorCallingConvention(len == 0 ? 0 : data[0]);
        }

        ULONG NumberOfTypeArguments() const {
            if (len > 1 &&
                (CallingConvention() & IMAGE_CEE_CS_CALLCONV_GENERIC) != 0) {
                auto temp = data + 1;
                return CorSigUncompressData(temp);
            }
            return 0;
        }

        ULONG NumberOfArguments() const {
            if (len > 2 &&
                (CallingConvention() & IMAGE_CEE_CS_CALLCONV_GENERIC) != 0) {
                auto temp = data + 2;
                return CorSigUncompressData(temp);
            }
            if (len > 1) {
                auto temp = data + 1;
                return CorSigUncompressData(temp);
            }
            return 0;
        }

        WSTRING str() const {
            return HexStr(data, len);
        }
    };

}

#endif  // CLR_PROFILER_CLRHELPER_H_
