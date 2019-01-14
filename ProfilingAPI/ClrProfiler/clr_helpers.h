#ifndef CLR_PROFILER_CLRHELPER_H_
#define CLR_PROFILER_CLRHELPER_H_

#include <iomanip>
#include <vector>
#include "string.h"

namespace trace {

    constexpr char HexMap[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

    std::wstring HexStr(unsigned char *data, int len)
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
        const int len = 0;

        MethodSignature(const unsigned char * data, const int len) : data(data),len(len) {}

        inline bool operator==(const MethodSignature& other) const {
            return memcmp(data, other.data, len);
        }

        CorCallingConvention CallingConvention() const {
            return CorCallingConvention(len == 0 ? 0 : data[0]);
        }

        size_t NumberOfTypeArguments() const {
            if (len > 1 &&
                (CallingConvention() & IMAGE_CEE_CS_CALLCONV_GENERIC) != 0) {
                return data[1];
            }
            return 0;
        }

        size_t NumberOfArguments() const {
            if (len > 2 &&
                (CallingConvention() & IMAGE_CEE_CS_CALLCONV_GENERIC) != 0) {
                return data[2];
            }
            if (len > 1) {
                return data[1];
            }
            return 0;
        }

        WSTRING str() const {
            return HexStr(const_cast<unsigned char *>(data), len);
        }
    };

}

#endif  // CLR_PROFILER_CLRHELPER_H_
