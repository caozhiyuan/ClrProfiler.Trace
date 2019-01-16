#include "clr_helpers.h"
#include "logging.h"

#undef IfFalseRet
#define IfFalseRet(EXPR)                       \
  do {                                        \
    if ((EXPR) == false) return S_FALSE; \
  } while (0)

namespace trace
{
    HRESULT MethodSignature::Init()
    {
        unsigned char elem_type;

        IfFalseRet(SigParseUtil::ParseByte(pbCur, pbEnd, &elem_type));

        unsigned gen_param_count;
        unsigned param_count;

        if (elem_type & IMAGE_CEE_CS_CALLCONV_GENERIC)
        {
            IfFalseRet(SigParseUtil::ParseNumber(pbCur, pbEnd, &gen_param_count));

            numberOfTypeArguments = gen_param_count;
        }

        IfFalseRet(SigParseUtil::ParseNumber(pbCur, pbEnd, &param_count));
        numberOfArguments = param_count;

        const PCCOR_SIGNATURE pbRet = pbCur;

        IfFalseRet(SigParseUtil::ParseRetType(pbCur, pbEnd));

        ret.pbBase = pbBase;

        ret.length = pbCur - pbRet;
        ret.offset = pbCur - pbBase - ret.length;

        params = std::vector<MethodArgument>(param_count);

        bool fEncounteredSentinal = false;
        for (unsigned i = 0; i < param_count; i++)
        {
            if (pbCur >= pbEnd)
                return S_FALSE;

            if (*pbCur == ELEMENT_TYPE_SENTINEL)
            {
                if (fEncounteredSentinal)
                    return S_FALSE;

                fEncounteredSentinal = true;

                pbCur++;
            }

            const PCCOR_SIGNATURE pbParam = pbCur;

            MethodArgument argument{};
            argument.pbBase = pbBase;

            IfFalseRet(SigParseUtil::ParseParam(pbCur, pbEnd));

            argument.length = pbCur - pbParam;
            argument.offset = pbCur - pbBase - argument.length;

            params.push_back(argument);
        }

        return S_OK;
    }

    bool MethodArgument::NeedBox() const
    {
        PCCOR_SIGNATURE pbCur = pbBase + offset;
        PCCOR_SIGNATURE pbEnd = pbCur + length;

        SigParseUtil::ParseOptionalCustomMods(pbCur, pbEnd);

        if (*pbCur == ELEMENT_TYPE_TYPEDBYREF)
        {
            pbCur++;
            return true;
        }

        if (*pbCur == ELEMENT_TYPE_BYREF)
        {
            pbCur++;
        }

        switch (*pbCur)
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
            return true;
        case  ELEMENT_TYPE_VALUETYPE:
            return true;
        case  ELEMENT_TYPE_GENERICINST:
            pbCur++;
            return *pbCur == ELEMENT_TYPE_VALUETYPE;
        case  ELEMENT_TYPE_MVAR:
        case  ELEMENT_TYPE_VAR:
            return true;
        default:
            return false;
        }
    }

    unsigned MethodRet::GetReturnType() const
    {
        PCCOR_SIGNATURE pbCur = pbBase + offset;
        PCCOR_SIGNATURE pbEnd = pbCur + length;

        SigParseUtil::ParseOptionalCustomMods(pbCur, pbEnd);

        unsigned retType = 0;
        if (*pbCur == ELEMENT_TYPE_TYPEDBYREF)
        {
            pbCur++;
            retType |= MethodRetType_NeedBox;
            return retType;
        }

        if (*pbCur == ELEMENT_TYPE_VOID)
        {
            pbCur++;
            retType |= MethodRetType_Void;
            return retType;
        }

        if (*pbCur == ELEMENT_TYPE_BYREF)
        {
            pbCur++;
        }

        switch (*pbCur)
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
            retType |= MethodRetType_NeedBox;
            break;
        case  ELEMENT_TYPE_VALUETYPE:
            retType |= MethodRetType_NeedBox;
            break;
        case  ELEMENT_TYPE_GENERICINST:
            pbCur++;
            if (*pbCur == ELEMENT_TYPE_VALUETYPE)
            {
                retType |= MethodRetType_NeedBox;
            }
            break;
        case  ELEMENT_TYPE_MVAR:
        case  ELEMENT_TYPE_VAR:
            retType |= MethodRetType_NeedBox;
            break;
        default: 
            break;
        }
        return  retType;
    }
}
