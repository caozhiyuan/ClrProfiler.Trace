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

#include "clr_helpers.h"
#include "logging.h"

#undef IfFalseRet
#define IfFalseRet(EXPR)                       \
  do {                                        \
    if ((EXPR) == false) return S_FALSE; \
  } while (0)

namespace trace
{
    bool ParseByte(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd, unsigned char *pbOut)
    {
        if (pbCur < pbEnd)
        {
            *pbOut = *pbCur;
            pbCur++;
            return true;
        }

        return false;
    }

    bool ParseNumber(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd, unsigned *pOut)
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


    bool ParseTypeDefOrRefEncoded(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd,
        unsigned char *pIndexTypeOut, unsigned *pIndexOut)
    {
        // parse an encoded typedef or typeref
        unsigned encoded = 0;

        if (!ParseNumber(pbCur, pbEnd, &encoded))
            return false;

        *pIndexTypeOut = (unsigned char)(encoded & 0x3);
        *pIndexOut = (encoded >> 2);
        return true;
    }

    bool ParseArrayShape(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd)
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

    /*  we don't support
        PTR CustomMod* VOID
        PTR CustomMod* Type
        FNPTR MethodDefSig
        FNPTR MethodRefSig
        CustomMod*
     */
    bool ParseType(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd)
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
       
           return false;

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

            return false;

        case  ELEMENT_TYPE_ARRAY:
            // ARRAY Type ArrayShape

            if (!ParseType(pbCur, pbEnd))
                return false;

            if (!ParseArrayShape(pbCur, pbEnd))
                return false;
            break;

        case  ELEMENT_TYPE_SZARRAY:
            // SZARRAY Type

            if (*pbCur == ELEMENT_TYPE_CMOD_OPT || *pbCur == ELEMENT_TYPE_CMOD_REQD) {
                return false;
            }

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

    // Param ::= CustomMod* ( TYPEDBYREF | [BYREF] Type ) 
    // CustomMod* TYPEDBYREF we don't support
    bool ParseParam(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd)
    {
        if (*pbCur == ELEMENT_TYPE_CMOD_OPT || *pbCur == ELEMENT_TYPE_CMOD_REQD) {
            return false;
        }

        if (pbCur >= pbEnd)
            return false;

        if (*pbCur == ELEMENT_TYPE_TYPEDBYREF)
        {
            return false;
        }

        if (*pbCur == ELEMENT_TYPE_BYREF)
        {
            pbCur++;
        }

        if (!ParseType(pbCur, pbEnd))
            return false;

        return true;
    }

    // RetType ::= CustomMod* ( VOID | TYPEDBYREF | [BYREF] Type ) 
    // CustomMod* TYPEDBYREF we don't support
    bool ParseRetType(PCCOR_SIGNATURE &pbCur, PCCOR_SIGNATURE pbEnd)
    {
        if (*pbCur == ELEMENT_TYPE_CMOD_OPT || *pbCur == ELEMENT_TYPE_CMOD_REQD) {
            return false;
        }

        if (pbCur >= pbEnd)
            return false;

        if (*pbCur == ELEMENT_TYPE_TYPEDBYREF)
        {
            return false;
        }

        if (*pbCur == ELEMENT_TYPE_VOID)
        {
            pbCur++;
            return true;
        }

        if (*pbCur == ELEMENT_TYPE_BYREF)
        {
            pbCur++;
        }

        if (!ParseType(pbCur, pbEnd))
            return false;

        return true;
    }

    HRESULT MethodSignature::TryParse() {

        PCCOR_SIGNATURE pbCur = pbBase;
        PCCOR_SIGNATURE pbEnd = pbBase + len;
        unsigned char elem_type;
        IfFalseRet(ParseByte(pbCur, pbEnd, &elem_type));
        if (elem_type & IMAGE_CEE_CS_CALLCONV_GENERIC) {
            unsigned gen_param_count;
            IfFalseRet(ParseNumber(pbCur, pbEnd, &gen_param_count));
            numberOfTypeArguments = gen_param_count;
        }

        unsigned param_count;
        IfFalseRet(ParseNumber(pbCur, pbEnd, &param_count));
        numberOfArguments = param_count;

        const PCCOR_SIGNATURE pbRet = pbCur;

        IfFalseRet(ParseRetType(pbCur, pbEnd));

        ret.pbBase = pbBase;
        ret.length = pbCur - pbRet;
        ret.offset = pbCur - pbBase - ret.length;
        params = std::vector<MethodArgument>(param_count);

        auto fEncounteredSentinal = false;
        for (unsigned i = 0; i < param_count; i++) {
            if (pbCur >= pbEnd)
                return S_FALSE;

            if (*pbCur == ELEMENT_TYPE_SENTINEL) {
                if (fEncounteredSentinal)
                    return S_FALSE;

                fEncounteredSentinal = true;
                pbCur++;
            }

            const PCCOR_SIGNATURE pbParam = pbCur;

            IfFalseRet(ParseParam(pbCur, pbEnd));

            MethodArgument argument{};
            argument.pbBase = pbBase;
            argument.length = pbCur - pbParam;
            argument.offset = pbCur - pbBase - argument.length;

            params.push_back(argument);
        }

        return S_OK;
    }

    MethodArgumentFlag MethodArgument::GetFlags() const {

        PCCOR_SIGNATURE pbCur = pbBase + offset;

        if (*pbCur == ELEMENT_TYPE_VOID) {
            pbCur++;
            return MethodArgumentFlag_VOID;
        }

        if (*pbCur == ELEMENT_TYPE_BYREF) {
            pbCur++;
        }

        MethodArgumentFlag flag = MethodArgumentFlag_None;
        switch (*pbCur) {
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
            flag = MethodArgumentFlag_BOX;
            break;
        case  ELEMENT_TYPE_VALUETYPE:
            flag = MethodArgumentFlag_BOX;
            break;
        case  ELEMENT_TYPE_GENERICINST:
            pbCur++;
            if (*pbCur == ELEMENT_TYPE_VALUETYPE) {
                flag = MethodArgumentFlag_BOX;
            }
            break;
        case  ELEMENT_TYPE_MVAR:
        case  ELEMENT_TYPE_VAR:
            flag = MethodArgumentFlag_BOX;
            break;
        default:
            break;
        }
        return  flag;
    }

    AssemblyInfo GetAssemblyInfo(ICorProfilerInfo3* info,
        const AssemblyID& assembly_id) {
        WCHAR name[kNameMaxSize];
        DWORD name_len = 0;
        auto hr = info->GetAssemblyInfo(assembly_id, kNameMaxSize, &name_len, name,
            nullptr, nullptr);
        if (FAILED(hr) || name_len == 0) {
            return {};
        }
        return { assembly_id, WSTRING(name) };
    }

    WSTRING GetAssemblyName(
        const CComPtr<IMetaDataAssemblyImport>& assembly_import) {
        mdAssembly current = mdAssemblyNil;
        auto hr = assembly_import->GetAssemblyFromScope(&current);
        if (FAILED(hr)) {
            return ""_W;
        }
        WCHAR name[kNameMaxSize];
        DWORD name_len = 0;
        ASSEMBLYMETADATA assembly_metadata{};
        DWORD assembly_flags = 0;
        hr = assembly_import->GetAssemblyProps(current, nullptr, nullptr, nullptr,
            name, kNameMaxSize, &name_len,
            &assembly_metadata, &assembly_flags);
        if (FAILED(hr) || name_len == 0) {
            return ""_W;
        }
        return WSTRING(name);
    }
    
    WSTRING GetAssemblyName(const CComPtr<IMetaDataAssemblyImport>& assembly_import,
        const mdAssemblyRef& assembly_ref) {
        WCHAR name[kNameMaxSize];
        DWORD name_len = 0;
        ASSEMBLYMETADATA assembly_metadata{};
        DWORD assembly_flags = 0;
        const auto hr = assembly_import->GetAssemblyRefProps(
            assembly_ref, nullptr, nullptr, name, kNameMaxSize, &name_len,
            &assembly_metadata, nullptr, nullptr, &assembly_flags);
        if (FAILED(hr) || name_len == 0) {
            return ""_W;
        }
        return WSTRING(name);
    }

    mdAssemblyRef FindAssemblyRef(
        const CComPtr<IMetaDataAssemblyImport>& assembly_import,
        const WSTRING& assembly_name) {
        for (mdAssemblyRef assembly_ref : EnumAssemblyRefs(assembly_import)) {
            if (GetAssemblyName(assembly_import, assembly_ref) == assembly_name) {
                return assembly_ref;
            }
        }
        return mdAssemblyRefNil;
    }

    ModuleInfo GetModuleInfo(ICorProfilerInfo3* info, const ModuleID& module_id) {
        const DWORD module_path_size = 260;
        WCHAR module_path[module_path_size]{};
        DWORD module_path_len = 0;
        LPCBYTE base_load_address;
        AssemblyID assembly_id = 0;
        DWORD module_flags = 0;
        const HRESULT hr = info->GetModuleInfo2(
            module_id, &base_load_address, module_path_size, &module_path_len,
            module_path, &assembly_id, &module_flags);
        if (FAILED(hr) || module_path_len == 0) {
            return {};
        }
        return { module_id, WSTRING(module_path), GetAssemblyInfo(info, assembly_id),
                module_flags };
    }
}
