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
#include "macros.h"

#undef IfFalseRetFAIL
#define IfFalseRetFAIL(EXPR)                       \
  do {                                        \
    if ((EXPR) == false) return E_FAIL; \
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
        IfFalseRetFAIL(ParseByte(pbCur, pbEnd, &elem_type));
        if (elem_type & IMAGE_CEE_CS_CALLCONV_GENERIC) {
            unsigned gen_param_count;
            IfFalseRetFAIL(ParseNumber(pbCur, pbEnd, &gen_param_count));
            numberOfTypeArguments = gen_param_count;
        }

        unsigned param_count;
        IfFalseRetFAIL(ParseNumber(pbCur, pbEnd, &param_count));
        numberOfArguments = param_count;

        const PCCOR_SIGNATURE pbRet = pbCur;

        IfFalseRetFAIL(ParseRetType(pbCur, pbEnd));

        ret.pbBase = pbBase;
        ret.length = (ULONG)(pbCur - pbRet);
        ret.offset = (ULONG)(pbCur - pbBase - ret.length);

        auto fEncounteredSentinal = false;
        for (unsigned i = 0; i < param_count; i++) {
            if (pbCur >= pbEnd)
                return E_FAIL;

            if (*pbCur == ELEMENT_TYPE_SENTINEL) {
                if (fEncounteredSentinal)
                    return E_FAIL;

                fEncounteredSentinal = true;
                pbCur++;
            }

            const PCCOR_SIGNATURE pbParam = pbCur;

            IfFalseRetFAIL(ParseParam(pbCur, pbEnd));

            MethodArgument argument{};
            argument.pbBase = pbBase;
            argument.length = (ULONG)(pbCur - pbParam);
            argument.offset = (ULONG)(pbCur - pbBase - argument.length);

            params.push_back(argument);
        }

        return S_OK;
    }

    bool MethodArgument::IsBoxedType() const {
       
        bool flag = false;
        PCCOR_SIGNATURE pbCur = &pbBase[offset];

        if (*pbCur == ELEMENT_TYPE_BYREF) {
            pbCur++;
        }

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
        case  ELEMENT_TYPE_VALUETYPE:
        case  ELEMENT_TYPE_MVAR:
        case  ELEMENT_TYPE_VAR:
            flag = true;
            break;
        case  ELEMENT_TYPE_GENERICINST:
            pbCur++;
            if (*pbCur == ELEMENT_TYPE_VALUETYPE) {
                flag = true;
            }
            break;
        default:
            break;
        }
        return flag;
    }

    mdToken MethodArgument::GetTypeTok(CComPtr<IMetaDataEmit2>& pEmit, 
        mdAssemblyRef corLibRef) const {

        mdToken token = mdTokenNil;
        PCCOR_SIGNATURE pbCur = &pbBase[offset];
        const PCCOR_SIGNATURE pTemp = pbCur;

        if (*pbCur == ELEMENT_TYPE_BYREF) {
            pbCur++;
        }

        switch (*pbCur) {
        case  ELEMENT_TYPE_BOOLEAN:
            pEmit->DefineTypeRefByName(corLibRef, L"System.Boolean", &token);
            break;
        case  ELEMENT_TYPE_CHAR:
            pEmit->DefineTypeRefByName(corLibRef, L"System.Char", &token);
            break;
        case  ELEMENT_TYPE_I1:
            pEmit->DefineTypeRefByName(corLibRef, L"System.Byte", &token);
            break;
        case  ELEMENT_TYPE_U1:
            pEmit->DefineTypeRefByName(corLibRef, L"System.SByte", &token);
            break;
        case  ELEMENT_TYPE_U2:
            pEmit->DefineTypeRefByName(corLibRef, L"System.UInt16", &token);
            break;
        case  ELEMENT_TYPE_I2:
            pEmit->DefineTypeRefByName(corLibRef, L"System.Int16", &token);
            break;
        case  ELEMENT_TYPE_I4:
            pEmit->DefineTypeRefByName(corLibRef, L"System.Int32", &token);
            break;
        case  ELEMENT_TYPE_U4:
            pEmit->DefineTypeRefByName(corLibRef, L"System.UInt32", &token);
            break;
        case  ELEMENT_TYPE_I8:
            pEmit->DefineTypeRefByName(corLibRef, L"System.Int64", &token);
            break;
        case  ELEMENT_TYPE_U8:
            pEmit->DefineTypeRefByName(corLibRef, L"System.UInt64", &token);
            break;
        case  ELEMENT_TYPE_R4:
            pEmit->DefineTypeRefByName(corLibRef, L"System.Single", &token);
            break;
        case  ELEMENT_TYPE_R8:
            pEmit->DefineTypeRefByName(corLibRef, L"System.Double", &token);
            break;
        case  ELEMENT_TYPE_I:
            pEmit->DefineTypeRefByName(corLibRef, L"System.IntPtr", &token);
            break;
        case  ELEMENT_TYPE_U:
            pEmit->DefineTypeRefByName(corLibRef, L"System.UIntPtr", &token);
            break;
        case  ELEMENT_TYPE_STRING:
            pEmit->DefineTypeRefByName(corLibRef, L"System.String", &token);
            break;
        case  ELEMENT_TYPE_OBJECT:
            pEmit->DefineTypeRefByName(corLibRef, L"System.Object", &token);
            break;
        case  ELEMENT_TYPE_CLASS:
            pbCur++;
            token = CorSigUncompressToken(pbCur);
        case  ELEMENT_TYPE_VALUETYPE:
            pbCur++;
            token = CorSigUncompressToken(pbCur);
            break;
        case  ELEMENT_TYPE_GENERICINST:
        case  ELEMENT_TYPE_SZARRAY:
        case  ELEMENT_TYPE_ARRAY:
        case  ELEMENT_TYPE_MVAR:
        case  ELEMENT_TYPE_VAR:
            pEmit->GetTokenFromTypeSpec(pbCur, length - static_cast<ULONG>(pbCur - pTemp), &token);
            break;
        default:
            break;
        }
        return token;
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

    mdAssemblyRef FindCorLibAssemblyRef(
        const CComPtr<IMetaDataAssemblyImport>& assembly_import) {
        for (mdAssemblyRef assembly_ref : EnumAssemblyRefs(assembly_import)) {
            auto name = GetAssemblyName(assembly_import, assembly_ref);
            if (name == L"System.Runtime" || name == L"mscorlib" || name == L"System.Private.CoreLib") {
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

    TypeInfo GetTypeInfo(const CComPtr<IMetaDataImport2>& metadata_import,
        const mdToken& token) {
        mdToken parent_token = mdTokenNil;
        WCHAR type_name[kNameMaxSize]{};
        DWORD type_name_len = 0;

        HRESULT hr = E_FAIL;
        const auto token_type = TypeFromToken(token);
        switch (token_type) {
        case mdtTypeDef:
            hr = metadata_import->GetTypeDefProps(token, type_name, kNameMaxSize,
                &type_name_len, nullptr, nullptr);
            break;
        case mdtTypeRef:
            hr = metadata_import->GetTypeRefProps(token, &parent_token, type_name,
                kNameMaxSize, &type_name_len);
            break;
        case mdtTypeSpec: {
            PCCOR_SIGNATURE signature{};
            ULONG signature_length{};

            hr = metadata_import->GetTypeSpecFromToken(token, &signature,
                &signature_length);
            
            if (FAILED(hr) || signature_length < 3) {
                return {};
            }

            if (signature[0] & ELEMENT_TYPE_GENERICINST) {
                mdToken type_token;
                CorSigUncompressToken(&signature[2], &type_token);
                return GetTypeInfo(metadata_import, type_token);
            }
        } break;
        case mdtModuleRef:
            metadata_import->GetModuleRefProps(token, type_name, kNameMaxSize,
                &type_name_len);
            break;
        case mdtMemberRef:
            return GetFunctionInfo(metadata_import, token).type;
            break;
        case mdtMethodDef:
            return GetFunctionInfo(metadata_import, token).type;
            break;
        }
        if (FAILED(hr) || type_name_len == 0) {
            return {};
        }

        return { token, WSTRING(type_name) };
    }

    FunctionInfo GetFunctionInfo(const CComPtr<IMetaDataImport2>& metadata_import,
        const mdToken& token) {

        mdToken parent_token = mdTokenNil;
        WCHAR function_name[kNameMaxSize]{};
        DWORD function_name_len = 0;

        PCCOR_SIGNATURE raw_signature;
        ULONG raw_signature_len;

        HRESULT hr = E_FAIL;
        const auto token_type = TypeFromToken(token);
        switch (token_type) {
        case mdtMemberRef:
            hr = metadata_import->GetMemberRefProps(
                token, &parent_token, function_name, kNameMaxSize, &function_name_len,
                &raw_signature, &raw_signature_len);
            break;
        case mdtMethodDef:
            hr = metadata_import->GetMemberProps(
                token, &parent_token, function_name, kNameMaxSize, &function_name_len,
                nullptr, &raw_signature, &raw_signature_len, nullptr, nullptr,
                nullptr, nullptr, nullptr);
            break;
        case mdtMethodSpec: {
            hr = metadata_import->GetMethodSpecProps(
                token, &parent_token, &raw_signature, &raw_signature_len);
            if (FAILED(hr)) {
                return {};
            }
            auto generic_info = GetFunctionInfo(metadata_import, parent_token);
            std::memcpy(function_name, generic_info.name.c_str(),
                sizeof(WCHAR) * (generic_info.name.length() + 1));
            function_name_len = (DWORD)(generic_info.name.length() + 1);
        } break;
        default:
            break;
        }
        if (FAILED(hr) || function_name_len == 0) {
            return {};
        }

        // parent_token could be: TypeDef, TypeRef, TypeSpec, ModuleRef, MethodDef
        const auto type_info = GetTypeInfo(metadata_import, parent_token);

        return { token, WSTRING(function_name), type_info,
                MethodSignature(raw_signature,raw_signature_len) };
    }

    //1. .net framework gac or net core DOTNET_ADDITIONAL_DEPS=%PROGRAMFILES%\dotnet\x64\additionalDeps\Datadog.Trace.ClrProfiler.Managed
    //2. just proj ref
    HRESULT GetProfilerAssemblyRef(CComPtr<IUnknown>& metadata_interfaces, mdAssemblyRef& assemblyRef) {

        auto pAssemblyImport = metadata_interfaces.As<IMetaDataAssemblyImport>(
            IID_IMetaDataAssemblyImport);
        auto pAssemblyEmit =
            metadata_interfaces.As<IMetaDataAssemblyEmit>(IID_IMetaDataAssemblyEmit);

        if (pAssemblyImport.IsNull() || pAssemblyEmit.IsNull()) {
            return E_FAIL;
        }

        mdAssembly assembly;
        HRESULT hr = pAssemblyImport->GetAssemblyFromScope(&assembly);
        IfFailRet(hr);

        auto token = FindAssemblyRef(pAssemblyImport, kProfilerAssemblyName);
        if (token != mdAssemblyRefNil) {
            assemblyRef = token;
            return S_OK;
        }

        BYTE rgbPublicKeyToken[] = { 0xde, 0xf8, 0x6d, 0x06, 0x1d, 0x0d, 0x2e, 0xeb };
        WCHAR wszLocale[MAX_PATH];
        wcscpy_s(wszLocale, L"neutral");

        ASSEMBLYMETADATA assemblyMetaData;
        ZeroMemory(&assemblyMetaData, sizeof(assemblyMetaData));
        assemblyMetaData.usMajorVersion = 0;
        assemblyMetaData.usMinorVersion = 6;
        assemblyMetaData.usBuildNumber = 0;
        assemblyMetaData.usRevisionNumber = 0;
        assemblyMetaData.szLocale = wszLocale;
        assemblyMetaData.cbLocale = _countof(wszLocale);

        hr = pAssemblyEmit->DefineAssemblyRef(
            (void *)rgbPublicKeyToken,
            sizeof(rgbPublicKeyToken),
            kProfilerAssemblyName,
            &assemblyMetaData,
            NULL,
            NULL,
            0,
            &assemblyRef);
        IfFailRet(hr);

        return  hr;
    }
}