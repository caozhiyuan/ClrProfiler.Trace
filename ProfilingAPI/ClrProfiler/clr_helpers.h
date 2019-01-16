
/***************************************************************************************************
*****************************                Signature                ******************************
****************************************************************************************************

Sig ::= MethodDefSig | MethodRefSig | StandAloneMethodSig | FieldSig | PropertySig | LocalVarSig

MethodDefSig ::= [[HASTHIS] [EXPLICITTHIS]] (DEFAULT|VARARG|GENERIC GenParamCount) ParamCount RetType Param*

MethodRefSig ::= [[HASTHIS] [EXPLICITTHIS]] VARARG ParamCount RetType Param* [SENTINEL Param+]

StandAloneMethodSig ::=  [[HASTHIS] [EXPLICITTHIS]] (DEFAULT|VARARG|C|STDCALL|THISCALL|FASTCALL)
                    ParamCount RetType Param* [SENTINEL Param+]

FieldSig ::= FIELD CustomMod* Type

PropertySig ::= PROPERTY [HASTHIS] ParamCount CustomMod* Type Param*

LocalVarSig ::= LOCAL_SIG Count (TYPEDBYREF | ([CustomMod] [Constraint])* [BYREF] Type)+


-------------

CustomMod ::= ( CMOD_OPT | CMOD_REQD ) ( TypeDefEncoded | TypeRefEncoded )

Constraint ::= #define ELEMENT_TYPE_PINNED

Param ::= CustomMod* ( TYPEDBYREF | [BYREF] Type )

RetType ::= CustomMod* ( VOID | TYPEDBYREF | [BYREF] Type )

Type ::= ( BOOLEAN | CHAR | I1 | U1 | U2 | U2 | I4 | U4 | I8 | U8 | R4 | R8 | I | U |
                | VALUETYPE TypeDefOrRefEncoded
                | CLASS TypeDefOrRefEncoded
                | STRING
                | OBJECT
                | PTR CustomMod* VOID
                | PTR CustomMod* Type
                | FNPTR MethodDefSig
                | FNPTR MethodRefSig
                | ARRAY Type ArrayShape
                | SZARRAY CustomMod* Type
                | GENERICINST (CLASS | VALUETYPE) TypeDefOrRefEncoded GenArgCount Type*
                | VAR Number
                | MVAR Number

ArrayShape ::= Rank NumSizes Size* NumLoBounds LoBound*

TypeDefOrRefEncoded ::= TypeDefEncoded | TypeRefEncoded
TypeDefEncoded ::= 32-bit-3-part-encoding-for-typedefs-and-typerefs
TypeRefEncoded ::= 32-bit-3-part-encoding-for-typedefs-and-typerefs

ParamCount ::= 29-bit-encoded-integer
GenArgCount ::= 29-bit-encoded-integer
Count ::= 29-bit-encoded-integer
Rank ::= 29-bit-encoded-integer
NumSizes ::= 29-bit-encoded-integer
Size ::= 29-bit-encoded-integer
NumLoBounds ::= 29-bit-encoded-integer
LoBounds ::= 29-bit-encoded-integer
Number ::= 29-bit-encoded-integer

***************************************************************************************************/


#ifndef CLR_PROFILER_CLRHELPER_H_
#define CLR_PROFILER_CLRHELPER_H_

#include <iomanip>
#include "string.h"
#include <vector>
#include "util.h"

namespace trace {

    class SigParseUtil
    {
    public:
        static bool ParseByte(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd, unsigned char *pbOut)
        {
            if (pbCur < pbEnd)
            {
                *pbOut = *pbCur;
                pbCur++;
                return true;
            }

            return false;
        }

        static bool ParseNumber(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd, unsigned *pOut)
        {
            // parse the variable length number format (0-4 bytes)

            unsigned char b1 = 0, b2 = 0, b3 = 0, b4 = 0;

            // at least one byte in the encoding, read that

            if (!ParseByte(pbCur, pbEnd, &b1))
                return false;

            if (b1 == 0xff)
            {
                // special encoding of 'NULL'
                // not sure what this means as a number, don't expect to see it except for string lengths
                // which we don't encounter anyway so calling it an error
                return false;
            }

            // early out on 1 byte encoding
            if ((b1 & 0x80) == 0)
            {
                *pOut = (int)b1;
                return true;
            }

            // now at least 2 bytes in the encoding, read 2nd byte
            if (!ParseByte(pbCur, pbEnd, &b2))
                return false;

            // early out on 2 byte encoding
            if ((b1 & 0x40) == 0)
            {
                *pOut = (((b1 & 0x3f) << 8) | b2);
                return true;
            }

            // must be a 4 byte encoding

            if ((b1 & 0x20) != 0)
            {
                // 4 byte encoding has this bit clear -- error if not
                return false;
            }

            if (!ParseByte(pbCur, pbEnd, &b3))
                return false;

            if (!ParseByte(pbCur, pbEnd, &b4))
                return false;

            *pOut = ((b1 & 0x1f) << 24) | (b2 << 16) | (b3 << 8) | b4;
            return true;
        }

        static bool ParseMethod(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd, unsigned char elem_type)
        {
            // MethodDefSig ::= [[HASTHIS] [EXPLICITTHIS]] (DEFAULT|VARARG|GENERIC GenParamCount)
            //                    ParamCount RetType Param* [SENTINEL Param+]

            unsigned gen_param_count;
            unsigned param_count;

            if (elem_type & IMAGE_CEE_CS_CALLCONV_GENERIC)
            {
                if (!ParseNumber(pbCur, pbEnd, &gen_param_count))
                    return false;
            }

            if (!ParseNumber(pbCur, pbEnd, &param_count))
                return false;

            if (!ParseRetType(pbCur, pbEnd))
                return false;

            bool fEncounteredSentinal = false;
            for (unsigned i = 0; i < param_count; i++)
            {
                if (pbCur >= pbEnd)
                    return false;

                if (*pbCur == ELEMENT_TYPE_SENTINEL)
                {
                    if (fEncounteredSentinal)
                        return false;

                    fEncounteredSentinal = true;

                    pbCur++;
                }

                if (!ParseParam(pbCur, pbEnd))
                    return false;
            }

            return true;
        }

        static bool ParseOptionalCustomMods(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd)
        {
            while (true)
            {
                if (pbCur >= pbEnd)
                    return true;

                switch (*pbCur)
                {
                case ELEMENT_TYPE_CMOD_OPT:
                case ELEMENT_TYPE_CMOD_REQD:
                    if (!ParseCustomMod(pbCur, pbEnd))
                        return false;
                    break;

                default:
                    return true;
                }
            }

            return false;
        }

        static bool ParseCustomMod(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd)
        {
            unsigned char cmod = 0;
            unsigned index;
            unsigned char indexType;

            if (!ParseByte(pbCur, pbEnd, &cmod))
                return false;

            if (cmod == ELEMENT_TYPE_CMOD_OPT || cmod == ELEMENT_TYPE_CMOD_REQD)
            {
                return ParseTypeDefOrRefEncoded(pbCur, pbEnd, &indexType, &index);
            }

            return false;
        }

        static bool ParseParam(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd)
        {
            // Param ::= CustomMod* ( TYPEDBYREF | [BYREF] Type )

            if (!ParseOptionalCustomMods(pbCur, pbEnd))
                return false;

            if (pbCur >= pbEnd)
                return false;

            if (*pbCur == ELEMENT_TYPE_TYPEDBYREF)
            {
                pbCur++;
                goto Success;
            }

            if (*pbCur == ELEMENT_TYPE_BYREF)
            {
                pbCur++;
            }

            if (!ParseType(pbCur, pbEnd))
                return false;

        Success:
            return true;
        }

        static bool ParseRetType(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd)
        {
            // RetType ::= CustomMod* ( VOID | TYPEDBYREF | [BYREF] Type )

            if (!ParseOptionalCustomMods(pbCur, pbEnd))
                return false;

            if (pbCur >= pbEnd)
                return false;

            if (*pbCur == ELEMENT_TYPE_TYPEDBYREF)
            {
                pbCur++;
                goto Success;
            }

            if (*pbCur == ELEMENT_TYPE_VOID)
            {
                pbCur++;
                goto Success;
            }

            if (*pbCur == ELEMENT_TYPE_BYREF)
            {
                pbCur++;
            }

            if (!ParseType(pbCur, pbEnd))
                return false;

        Success:
            return true;
        }

        static bool ParseArrayShape(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd)
        {
            unsigned rank;
            unsigned numsizes;
            unsigned size;

            // ArrayShape ::= Rank NumSizes Size* NumLoBounds LoBound*
            if (!ParseNumber(pbCur, pbEnd, &rank))
                return false;

            if (!ParseNumber(pbCur, pbEnd, &numsizes))
                return false;

            for (unsigned i = 0; i < numsizes; i++)
            {
                if (!ParseNumber(pbCur, pbEnd, &size))
                    return false;
            }

            if (!ParseNumber(pbCur, pbEnd, &numsizes))
                return false;

            for (unsigned i = 0; i < numsizes; i++)
            {
                if (!ParseNumber(pbCur, pbEnd, &size))
                    return false;
            }
            return true;
        }

        static bool ParseType(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd)
        {
            /*
            Type ::= ( BOOLEAN | CHAR | I1 | U1 | U2 | U2 | I4 | U4 | I8 | U8 | R4 | R8 | I | U |
            | VALUETYPE TypeDefOrRefEncoded
            | CLASS TypeDefOrRefEncoded
            | STRING
            | OBJECT
            | PTR CustomMod* VOID
            | PTR CustomMod* Type
            | FNPTR MethodDefSig
            | FNPTR MethodRefSig
            | ARRAY Type ArrayShape
            | SZARRAY CustomMod* Type
            | GENERICINST (CLASS | VALUETYPE) TypeDefOrRefEncoded GenArgCount Type *
            | VAR Number
            | MVAR Number

            */

            unsigned char elem_type;
            unsigned index;
            unsigned number;
            unsigned char indexType;

            if (!ParseByte(pbCur, pbEnd, &elem_type))
                return false;

            switch (elem_type)
            {
            case  ELEMENT_TYPE_BOOLEAN:
            case  ELEMENT_TYPE_CHAR:
            case  ELEMENT_TYPE_I1:
            case  ELEMENT_TYPE_U1:
            case  ELEMENT_TYPE_U2:
            case  ELEMENT_TYPE_I2:
            case  ELEMENT_TYPE_I4:
            case  ELEMENT_TYPE_U4:
            case  ELEMENT_TYPE_I8:
            case  ELEMENT_TYPE_U8:
            case  ELEMENT_TYPE_R4:
            case  ELEMENT_TYPE_R8:
            case  ELEMENT_TYPE_I:
            case  ELEMENT_TYPE_U:
            case  ELEMENT_TYPE_STRING:
            case  ELEMENT_TYPE_OBJECT:
                // simple types
                break;

            case  ELEMENT_TYPE_PTR:
                // PTR CustomMod* VOID
                // PTR CustomMod* Type


                if (!ParseOptionalCustomMods(pbCur, pbEnd))
                    return false;

                if (pbCur >= pbEnd)
                    return false;

                if (*pbCur == ELEMENT_TYPE_VOID)
                {
                    pbCur++;
                    break;
                }

                if (!ParseType(pbCur, pbEnd))
                    return false;

                break;

            case  ELEMENT_TYPE_CLASS:
                // CLASS TypeDefOrRefEncoded

                if (!ParseTypeDefOrRefEncoded(pbCur, pbEnd, &indexType, &index))
                    return false;
                break;

            case  ELEMENT_TYPE_VALUETYPE:
                //VALUETYPE TypeDefOrRefEncoded

                if (!ParseTypeDefOrRefEncoded(pbCur, pbEnd, &indexType, &index))
                    return false;

                break;

            case  ELEMENT_TYPE_FNPTR:
                // FNPTR MethodDefSig
                // FNPTR MethodRefSig

                if (!ParseByte(pbCur, pbEnd, &elem_type))
                    return false;

                if (!ParseMethod(pbCur, pbEnd, elem_type))
                    return false;

                break;

            case  ELEMENT_TYPE_ARRAY:
                // ARRAY Type ArrayShape

                if (!ParseType(pbCur, pbEnd))
                    return false;

                if (!ParseArrayShape(pbCur, pbEnd))
                    return false;
                break;

            case  ELEMENT_TYPE_SZARRAY:
                // SZARRAY CustomMod* Type

                if (!ParseOptionalCustomMods(pbCur, pbEnd))
                    return false;

                if (!ParseType(pbCur, pbEnd))
                    return false;

                break;

            case  ELEMENT_TYPE_GENERICINST:
                // GENERICINST (CLASS | VALUETYPE) TypeDefOrRefEncoded GenArgCount Type *

                if (!ParseByte(pbCur, pbEnd, &elem_type))
                    return false;

                if (elem_type != ELEMENT_TYPE_CLASS && elem_type != ELEMENT_TYPE_VALUETYPE)
                    return false;

                if (!ParseTypeDefOrRefEncoded(pbCur, pbEnd, &indexType, &index))
                    return false;

                if (!ParseNumber(pbCur, pbEnd, &number))
                    return false;

                for (unsigned i = 0; i < number; i++)
                {
                    if (!ParseType(pbCur, pbEnd))
                        return false;
                }

                break;

            case  ELEMENT_TYPE_VAR:
                // VAR Number
                if (!ParseNumber(pbCur, pbEnd, &number))
                    return false;

                break;

            case  ELEMENT_TYPE_MVAR:
                // MVAR Number
                if (!ParseNumber(pbCur, pbEnd, &number))
                    return false;

                break;
            }

            return true;
        }

        static bool ParseTypeDefOrRefEncoded(PCCOR_SIGNATURE &pbCur,
            PCCOR_SIGNATURE pbEnd, 
            unsigned char *pIndexTypeOut,
            unsigned *pIndexOut)
        {
            // parse an encoded typedef or typeref

            unsigned encoded = 0;

            if (!ParseNumber(pbCur, pbEnd, &encoded))
                return false;

            *pIndexTypeOut = (unsigned char)(encoded & 0x3);
            *pIndexOut = (encoded >> 2);
            return true;
        }
    };

    struct MethodArgument
    {
        INT64 offset;
        INT64 length;
        PCCOR_SIGNATURE pbBase;
        bool NeedBox() const;
    };

    enum MethodRetType
    {
        MethodRetType_Void = 1,
        MethodRetType_NeedBox = 2,
        MethodRetType_NotSupport = 3
    };

    struct MethodRet
    {
        INT64 offset;
        INT64 length;
        PCCOR_SIGNATURE pbBase;
        unsigned GetReturnType() const;
    };

    struct MethodSignature {
    private:
        PCCOR_SIGNATURE pbBase;
        PCCOR_SIGNATURE pbCur;
        PCCOR_SIGNATURE pbEnd;

        unsigned len;
        
        ULONG numberOfTypeArguments = 0;
        ULONG numberOfArguments = 0;
        
        MethodRet ret{};
        std::vector<MethodArgument> params;

    public:

        MethodSignature(PCCOR_SIGNATURE pb, const int cbBuffer)
        {
            pbBase = pb;
            pbCur = pb;
            pbEnd = pbBase + cbBuffer;
            len = cbBuffer;
        }
        
        HRESULT Init();

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
    };

}

#endif  // CLR_PROFILER_CLRHELPER_H_
