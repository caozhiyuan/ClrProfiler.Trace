// Copyright (c) .NET Foundation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "CorProfiler.h"
#include "CComPtr.h"
#include "logging.h"  // NOLINT
#include "corhlpr.h"
#include "macros.h"
#include "clr_helpers.h"
#include "config_loader.h"
#include "il_rewriter.h"
#include "il_rewriter_wrapper.h"
#include <string>
#include <vector>
#include <cassert>

namespace trace {

    CorProfiler::CorProfiler() : refCount(0), corProfilerInfo(nullptr)
    {
        Info("CorProfiler()");
    }

    CorProfiler::~CorProfiler()
    {
        if (this->corProfilerInfo != nullptr)
        {
            this->corProfilerInfo->Release();
            this->corProfilerInfo = nullptr;
        }
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::Initialize(IUnknown *pICorProfilerInfoUnk)
    {
        const HRESULT queryHR = pICorProfilerInfoUnk->QueryInterface(__uuidof(ICorProfilerInfo8), reinterpret_cast<void **>(&this->corProfilerInfo));

        if (FAILED(queryHR))
        {
            return E_FAIL;
        }

        const DWORD eventMask = COR_PRF_MONITOR_JIT_COMPILATION |
            COR_PRF_DISABLE_TRANSPARENCY_CHECKS_UNDER_FULL_TRUST | /* helps the case where this profiler is used on Full CLR */
            COR_PRF_DISABLE_INLINING |
            COR_PRF_MONITOR_MODULE_LOADS |
            COR_PRF_DISABLE_ALL_NGEN_IMAGES;

        this->corProfilerInfo->SetEventMask(eventMask);

        this->clrProfilerHomeEnvValue = GetEnvironmentValue(ClrProfilerHome);

        if(this->clrProfilerHomeEnvValue.empty()) {
            return E_FAIL;
        }

        this->traceAssemblies = LoadTraceAssemblies(this->clrProfilerHomeEnvValue);
        if (this->traceAssemblies.empty()) {
            return E_FAIL;
        }

        Info("CorProfiler Initialize Success");

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::Shutdown()
    {
        if (this->corProfilerInfo != nullptr)
        {
            this->corProfilerInfo->Release();
            this->corProfilerInfo = nullptr;
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainCreationStarted(AppDomainID appDomainId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainCreationFinished(AppDomainID appDomainId, HRESULT hrStatus)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainShutdownStarted(AppDomainID appDomainId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainShutdownFinished(AppDomainID appDomainId, HRESULT hrStatus)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyLoadStarted(AssemblyID assemblyId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyLoadFinished(AssemblyID assemblyId, HRESULT hrStatus)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyUnloadStarted(AssemblyID assemblyId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyUnloadFinished(AssemblyID assemblyId, HRESULT hrStatus)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ModuleLoadStarted(ModuleID moduleId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ModuleLoadFinished(ModuleID moduleId, HRESULT hrStatus) 
    {
        auto module_info = GetModuleInfo(this->corProfilerInfo, moduleId);
        if (!module_info.IsValid() || module_info.IsWindowsRuntime()) {
            return S_OK;
        }        
        const auto entryPointToken = module_info.GetEntryPointToken();
        ModuleMetaInfo* module_metadata = new ModuleMetaInfo(entryPointToken, module_info.assembly.name);
        {
            std::lock_guard<std::mutex> guard(mapLock);
            moduleMetaInfoMap[moduleId] = module_metadata;
        }

        if (entryPointToken != mdTokenNil)
        {
            Info("Assembly:{} EntryPointToken:{}", ToString(module_info.assembly.name), std::to_string(entryPointToken));
        }

        if (module_info.assembly.name == "mscorlib"_W || module_info.assembly.name == "System.Private.CoreLib"_W) {
                                  
            if(!corAssemblyProperty.szName.empty()) {
                return S_OK;
            }

            CComPtr<IUnknown> metadata_interfaces;
            auto hr = corProfilerInfo->GetModuleMetaData(moduleId, ofRead | ofWrite,
                IID_IMetaDataImport2,
                metadata_interfaces.GetAddressOf());
            RETURN_OK_IF_FAILED(hr);

            auto pAssemblyImport = metadata_interfaces.As<IMetaDataAssemblyImport>(
                IID_IMetaDataAssemblyImport);
            if (pAssemblyImport.IsNull()) {
                return S_OK;
            }

            mdAssembly assembly;
            hr = pAssemblyImport->GetAssemblyFromScope(&assembly);
            RETURN_OK_IF_FAILED(hr);

            hr = pAssemblyImport->GetAssemblyProps(
                assembly,
                &corAssemblyProperty.ppbPublicKey,
                &corAssemblyProperty.pcbPublicKey,
                &corAssemblyProperty.pulHashAlgId,
                NULL,
                0,
                NULL,
                &corAssemblyProperty.pMetaData,
                &corAssemblyProperty.assemblyFlags);
            RETURN_OK_IF_FAILED(hr);

            corAssemblyProperty.szName = module_info.assembly.name;

            auto pImport = metadata_interfaces.As<IMetaDataImport2>(IID_IMetaDataImport);
            auto pEmit = metadata_interfaces.As<IMetaDataEmit2>(IID_IMetaDataEmit);
            if (pEmit.IsNull() || pImport.IsNull()) {
                return S_OK;
            }

            mdModule module;
            hr = pImport->GetModuleFromScope(&module);
            RETURN_OK_IF_FAILED(hr);

            mdTypeDef assemblyTypeDef;
            hr = pImport->FindTypeDefByName(AssemblyTypeName.data(), NULL, &assemblyTypeDef);
            RETURN_OK_IF_FAILED(hr);
         
            auto enumerator = EnumMembersWithName(pImport, assemblyTypeDef, AssemblyLoadMethodName.data());
            for (auto assemblyLoadMethodDef : enumerator)
            {
                PCCOR_SIGNATURE raw_signature;
                ULONG raw_signature_len;
                DWORD       pdwAttr;
                ULONG       pulCodeRVA;
                DWORD       pdwImplFlags;
                hr = pImport->GetMethodProps(
                    assemblyLoadMethodDef,
                    NULL,
                    NULL,
                    0,
                    NULL,
                    &pdwAttr,
                    &raw_signature,
                    &raw_signature_len,
                    &pulCodeRVA,
                    &pdwImplFlags);
                RETURN_OK_IF_FAILED(hr);

                auto signature = MethodSignature(raw_signature, raw_signature_len);
                hr = signature.TryParse();
                RETURN_OK_IF_FAILED(hr);

                if (signature.NumberOfArguments() == 1 && !customLoadFromInit) {

                    // if don't define a custom load from , will raise cor lib bad image load
                    COR_SIGNATURE customAssemblyLoadSig[] =
                    {
                       IMAGE_CEE_CS_CALLCONV_DEFAULT ,
                       0x01,
                       ELEMENT_TYPE_VOID,
                       ELEMENT_TYPE_STRING
                    };
                    mdMethodDef customAssemblyLoadMethodDef;
                    hr = pEmit->DefineMethod(
                        assemblyTypeDef,
                        AssemblyCustomLoadMethodName.data(),
                        pdwAttr,
                        customAssemblyLoadSig,
                        sizeof(customAssemblyLoadSig),
                        pulCodeRVA,
                        pdwImplFlags,
                        &customAssemblyLoadMethodDef);
                    RETURN_OK_IF_FAILED(hr);

                    CComPtr<IMethodMalloc>  pIMethodMalloc;
                    hr = corProfilerInfo->GetILFunctionBodyAllocator(moduleId, pIMethodMalloc.GetAddressOf());
                    RETURN_OK_IF_FAILED(hr);

                    auto pwMethodBytes = (LPBYTE)pIMethodMalloc->Alloc(9);
                    const ULONG codeSize = 8;
                    ULONG offset = 0;
                    pwMethodBytes[offset++] = (BYTE)(CorILMethod_TinyFormat | codeSize << 2);
                    pwMethodBytes[offset++] = CEE_LDARG_0;
                    pwMethodBytes[offset++] = CEE_CALL;
                    *(UNALIGNED INT32*)&(pwMethodBytes[offset]) = assemblyLoadMethodDef;
                    offset += 4;
                    pwMethodBytes[offset++] = (BYTE)CEE_POP;
                    pwMethodBytes[offset] = (BYTE)CEE_RET;

                    hr = corProfilerInfo->SetILFunctionBody(moduleId, customAssemblyLoadMethodDef, pwMethodBytes);
                    RETURN_OK_IF_FAILED(hr);

                    customLoadFromInit = true;
                    break;
                }
            }

            return S_OK;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ModuleUnloadStarted(ModuleID moduleId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ModuleUnloadFinished(ModuleID moduleId, HRESULT hrStatus)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ModuleAttachedToAssembly(ModuleID moduleId, AssemblyID AssemblyId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ClassLoadStarted(ClassID classId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ClassLoadFinished(ClassID classId, HRESULT hrStatus)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ClassUnloadStarted(ClassID classId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ClassUnloadFinished(ClassID classId, HRESULT hrStatus)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::FunctionUnloadStarted(FunctionID functionId)
    {
        return S_OK;
    }

    // add ret ex methodTrace var to local var
    HRESULT ModifyLocalSig(CComPtr<IMetaDataImport2>& pImport,
        CComPtr<IMetaDataEmit2>& pEmit,
        ILRewriter& reWriter, 
        mdTypeRef exTypeRef,
        mdTypeRef methodTraceTypeRef)
    {
        HRESULT hr;
        PCCOR_SIGNATURE rgbOrigSig = NULL;
        ULONG cbOrigSig = 0;
        UNALIGNED INT32 temp = 0;
        if (reWriter.m_tkLocalVarSig != mdTokenNil)
        {
            IfFailRet(pImport->GetSigFromToken(reWriter.m_tkLocalVarSig, &rgbOrigSig, &cbOrigSig));

            //Check Is ReWrite or not
            const auto len = CorSigCompressToken(methodTraceTypeRef, &temp);
            if(cbOrigSig - len > 0){
                if(rgbOrigSig[cbOrigSig - len -1]== ELEMENT_TYPE_CLASS){
                    if (memcmp(&rgbOrigSig[cbOrigSig - len], &temp, len) == 0) {
                        return E_FAIL;
                    }
                }
            }
        }

        auto exTypeRefSize = CorSigCompressToken(exTypeRef, &temp);
        auto methodTraceTypeRefSize = CorSigCompressToken(methodTraceTypeRef, &temp);
        ULONG cbNewSize = cbOrigSig + 1 + 1 + methodTraceTypeRefSize + 1 + exTypeRefSize;
        ULONG cOrigLocals;
        ULONG cNewLocalsLen;
        ULONG cbOrigLocals = 0;

        if (cbOrigSig == 0) {
            cbNewSize += 2;
            reWriter.cNewLocals = 3;
            cNewLocalsLen = CorSigCompressData(reWriter.cNewLocals, &temp);
        }
        else {
            cbOrigLocals = CorSigUncompressData(rgbOrigSig + 1, &cOrigLocals);
            reWriter.cNewLocals = cOrigLocals + 3;
            cNewLocalsLen = CorSigCompressData(reWriter.cNewLocals, &temp);
            cbNewSize += cNewLocalsLen - cbOrigLocals;
        }

        const auto rgbNewSig = new COR_SIGNATURE[cbNewSize];
        *rgbNewSig = IMAGE_CEE_CS_CALLCONV_LOCAL_SIG;

        ULONG rgbNewSigOffset = 1;
        memcpy(rgbNewSig + rgbNewSigOffset, &temp, cNewLocalsLen);
        rgbNewSigOffset += cNewLocalsLen;

        if (cbOrigSig > 0) {
            const auto cbOrigCopyLen = cbOrigSig - 1 - cbOrigLocals;
            memcpy(rgbNewSig + rgbNewSigOffset, rgbOrigSig + 1 + cbOrigLocals, cbOrigCopyLen);
            rgbNewSigOffset += cbOrigCopyLen;
        }

        rgbNewSig[rgbNewSigOffset++] = ELEMENT_TYPE_OBJECT;
        rgbNewSig[rgbNewSigOffset++] = ELEMENT_TYPE_CLASS;
        exTypeRefSize = CorSigCompressToken(exTypeRef, &temp);
        memcpy(rgbNewSig + rgbNewSigOffset, &temp, exTypeRefSize);
        rgbNewSigOffset += exTypeRefSize;
        rgbNewSig[rgbNewSigOffset++] = ELEMENT_TYPE_CLASS;
        methodTraceTypeRefSize = CorSigCompressToken(methodTraceTypeRef, &temp);
        memcpy(rgbNewSig + rgbNewSigOffset, &temp, methodTraceTypeRefSize);
        rgbNewSigOffset += methodTraceTypeRefSize;

        IfFailRet(pEmit->GetTokenFromSig(&rgbNewSig[0], cbNewSize, &reWriter.m_tkLocalVarSig));

        return S_OK;
    }

    bool CorProfiler::FunctionIsNeedTrace(CComPtr<IMetaDataImport2>& pImport, ModuleMetaInfo* moduleMetaInfo, FunctionInfo functionInfo)
    {
        auto isTrace = false;
        for (const auto& assembly : traceAssemblies) {
            if (assembly.assemblyName == moduleMetaInfo->assemblyName && functionInfo.type.name == assembly.className) {
                for (const auto& method : assembly.methods) {
                    if (method.methodName == functionInfo.name) {
                        auto paramIsMatch = true;
                        if (!method.paramsName.empty()) {
                            auto paramNames = Split(method.paramsName, static_cast<wchar_t>(','));
                            auto arguments = functionInfo.signature.GetMethodArguments();
                            if(!arguments.empty()){
                                for (unsigned i = 0; i < arguments.size(); i++) {
                                    auto typeTokName = arguments[i].GetTypeTokName(pImport);
                                    if (typeTokName != paramNames[i])
                                    {
                                        paramIsMatch = false;
                                        break;
                                    }
                                }
                            }
                            else{
                                paramIsMatch = false;
                            }
                        }
                        if (paramIsMatch) {
                            isTrace = true;
                            break;
                        }
                    }
                }
            }
            if (isTrace) 
                break;
        }
        return isTrace;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::JITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock)
    {
        mdToken function_token = mdTokenNil;
        ModuleID moduleId;
        auto hr = corProfilerInfo->GetFunctionInfo(functionId, NULL, &moduleId, &function_token);
        RETURN_OK_IF_FAILED(hr);

        ModuleMetaInfo* moduleMetaInfo = nullptr;
        {
            std::lock_guard<std::mutex> guard(mapLock);
            if (moduleMetaInfoMap.count(moduleId) > 0) {
                moduleMetaInfo = moduleMetaInfoMap[moduleId];
            }
        }
        if(moduleMetaInfo == nullptr) {
            return S_OK;
        }

        bool isiLRewrote = false;
        {
            std::lock_guard<std::mutex> guard(mapLock);
            if (iLRewriteMap.count(function_token) > 0) {
                isiLRewrote = true;
            }
        }

        if (isiLRewrote) {
            return S_OK;
        }

        CComPtr<IUnknown> metadata_interfaces;
        hr = corProfilerInfo->GetModuleMetaData(moduleId, ofRead | ofWrite,
            IID_IMetaDataImport2,
            metadata_interfaces.GetAddressOf());
        RETURN_OK_IF_FAILED(hr);

        auto pImport = metadata_interfaces.As<IMetaDataImport2>(IID_IMetaDataImport);
        auto pEmit = metadata_interfaces.As<IMetaDataEmit2>(IID_IMetaDataEmit);
        if (pEmit.IsNull() || pImport.IsNull()) {
            return S_OK;
        }

        mdModule module;
        hr = pImport->GetModuleFromScope(&module);
        RETURN_OK_IF_FAILED(hr);

        auto functionInfo = GetFunctionInfo(pImport, function_token);
        if (!functionInfo.IsValid()) {
            return S_OK;
        }

        //.net framework need add gac 
        //.net core add premain il
        if (corAssemblyProperty.szName != "mscorlib"_W &&
            !entryPointReWrote &&
            functionInfo.id == moduleMetaInfo->entryPointToken)
        {
            const mdAssemblyRef corLibAssemblyRef = GetCorLibAssemblyRef(metadata_interfaces, corAssemblyProperty);
            if (corLibAssemblyRef == mdAssemblyRefNil) {
                return S_OK;
            }

            mdTypeRef assemblyTypeRef;
            hr = pEmit->DefineTypeRefByName(
                corLibAssemblyRef,
                AssemblyTypeName.data(),
                &assemblyTypeRef);
            RETURN_OK_IF_FAILED(hr);
            COR_SIGNATURE assemblyLoadSig[] =
            {
                IMAGE_CEE_CS_CALLCONV_DEFAULT ,
                0x01,
                ELEMENT_TYPE_VOID,
                ELEMENT_TYPE_STRING
            };
            mdMemberRef assemblyLoadMemberRef;
            hr = pEmit->DefineMemberRef(
                assemblyTypeRef,
                AssemblyCustomLoadMethodName.data(),
                assemblyLoadSig,
                sizeof(assemblyLoadSig),
                &assemblyLoadMemberRef);

            mdString profilerTraceDllNameTextToken;
            auto clrProfilerTraceDllName = clrProfilerHomeEnvValue + PathSeparator + ClrProfilerDllName;
            hr = pEmit->DefineUserString(clrProfilerTraceDllName.data(), (ULONG)clrProfilerTraceDllName.length(), &profilerTraceDllNameTextToken);
            RETURN_OK_IF_FAILED(hr);

            ILRewriter rewriter(corProfilerInfo, NULL, moduleId, function_token);
            RETURN_OK_IF_FAILED(rewriter.Import());

            auto pReWriter = &rewriter;
            ILRewriterWrapper reWriterWrapper(pReWriter);
            ILInstr * pFirstOriginalInstr = pReWriter->GetILList()->m_pNext;
            reWriterWrapper.SetILPosition(pFirstOriginalInstr);
            reWriterWrapper.LoadStr(profilerTraceDllNameTextToken);
            reWriterWrapper.CallMember(assemblyLoadMemberRef, false);
            hr = rewriter.Export();
            RETURN_OK_IF_FAILED(hr);

            {
                std::lock_guard<std::mutex> guard(mapLock);
                iLRewriteMap[function_token] = true;
            }
            entryPointReWrote = true;
            return S_OK;
        }

        hr = functionInfo.signature.TryParse();
        RETURN_OK_IF_FAILED(hr);

        if (!(functionInfo.signature.CallingConvention() & IMAGE_CEE_CS_CALLCONV_HASTHIS)) {
            return S_OK;
        }

        if(!FunctionIsNeedTrace(pImport, moduleMetaInfo, functionInfo))
        {
            return S_OK;
        }

        //return ref not support
        unsigned elementType;
        auto retTypeFlags = functionInfo.signature.GetRet().GetTypeFlags(elementType);
        if (retTypeFlags & TypeFlagByRef) {
            return S_OK;
        }

        mdAssemblyRef assemblyRef;
        hr = GetProfilerAssemblyRef(metadata_interfaces, assemblyRef);
        RETURN_OK_IF_FAILED(hr);

        mdTypeRef traceAgentTypeRef;
        hr = pEmit->DefineTypeRefByName(
            assemblyRef,
            TraceAgentTypeName.data(),
            &traceAgentTypeRef);
        RETURN_OK_IF_FAILED(hr);

        COR_SIGNATURE traceInstanceSig[] =
        {
            IMAGE_CEE_CS_CALLCONV_DEFAULT,
            0x00,
            ELEMENT_TYPE_OBJECT
        };

        mdMemberRef getInstanceMemberRef;
        hr = pEmit->DefineMemberRef(
            traceAgentTypeRef,
            GetInstanceMethodName.data(),
            traceInstanceSig,
            sizeof(traceInstanceSig),
            &getInstanceMemberRef);
        RETURN_OK_IF_FAILED(hr);

        mdTypeRef methodTraceTypeRef;
        hr = pEmit->DefineTypeRefByName(
            assemblyRef,
            MethodTraceTypeName.data(),
            &methodTraceTypeRef);
        RETURN_OK_IF_FAILED(hr);

        COR_SIGNATURE traceBeforeSig[] =
        {
            IMAGE_CEE_CS_CALLCONV_DEFAULT | IMAGE_CEE_CS_CALLCONV_HASTHIS ,
            0x05 ,
            ELEMENT_TYPE_OBJECT,
            ELEMENT_TYPE_STRING ,
            ELEMENT_TYPE_STRING,
            ELEMENT_TYPE_OBJECT,
            ELEMENT_TYPE_SZARRAY,
            ELEMENT_TYPE_OBJECT,
            ELEMENT_TYPE_U4
        };

        mdMemberRef beforeMemberRef;
        hr = pEmit->DefineMemberRef(
            traceAgentTypeRef,
            BeforeMethodName.data(),
            traceBeforeSig,
            sizeof(traceBeforeSig),
            &beforeMemberRef);
        RETURN_OK_IF_FAILED(hr);

        COR_SIGNATURE traceEndSig[] =
        {
            IMAGE_CEE_CS_CALLCONV_DEFAULT | IMAGE_CEE_CS_CALLCONV_HASTHIS,
            0x02,
            ELEMENT_TYPE_VOID,
            ELEMENT_TYPE_OBJECT,
            ELEMENT_TYPE_OBJECT
        };
        mdMemberRef endMemberRef;
        hr = pEmit->DefineMemberRef(
            methodTraceTypeRef,
            EndMethodName.data(),
            traceEndSig,
            sizeof(traceEndSig),
            &endMemberRef);
        RETURN_OK_IF_FAILED(hr);

        mdAssemblyRef corLibAssemblyRef = GetCorLibAssemblyRef(metadata_interfaces, corAssemblyProperty);
        if (corLibAssemblyRef == mdAssemblyRefNil) {
            return S_OK;
        }

        mdTypeRef exTypeRef;
        hr = pEmit->DefineTypeRefByName(
            corLibAssemblyRef,
            SystemException.data(),
            &exTypeRef);
        RETURN_OK_IF_FAILED(hr);

        ILRewriter rewriter(corProfilerInfo, NULL, moduleId, function_token);
        RETURN_OK_IF_FAILED(rewriter.Import());

        //ModifyLocalSig
        hr = ModifyLocalSig(pImport, pEmit, rewriter, exTypeRef, methodTraceTypeRef);
        RETURN_OK_IF_FAILED(hr);

        //add try catch finally
        auto pReWriter = &rewriter;
        mdTypeRef objectTypeRef;
        hr = pEmit->DefineTypeRefByName(
            corLibAssemblyRef,
            SystemObject.data(),
            &objectTypeRef);
        RETURN_OK_IF_FAILED(hr);

        mdString typeNameTextToken;
        auto typeName = functionInfo.type.name.data();
        hr = pEmit->DefineUserString(typeName, (ULONG)functionInfo.type.name.size(), &typeNameTextToken);
        RETURN_OK_IF_FAILED(hr);

        auto indexRet = rewriter.cNewLocals - 3;
        auto indexEx = rewriter.cNewLocals - 2;
        auto indexMethodTrace = rewriter.cNewLocals - 1;

        mdString methodNameTextToken;
        auto methodName = functionInfo.name.data();
        hr = pEmit->DefineUserString(methodName, (ULONG)functionInfo.name.size(), &methodNameTextToken);
        RETURN_OK_IF_FAILED(hr);
        ILRewriterWrapper reWriterWrapper(pReWriter);
        ILInstr * pFirstOriginalInstr = pReWriter->GetILList()->m_pNext;
        reWriterWrapper.SetILPosition(pFirstOriginalInstr);
        reWriterWrapper.LoadNull();
        reWriterWrapper.StLocal(indexMethodTrace);
        reWriterWrapper.LoadNull();
        reWriterWrapper.StLocal(indexEx);
        reWriterWrapper.LoadNull();
        reWriterWrapper.StLocal(indexRet);
        ILInstr* pTryStartInstr = reWriterWrapper.CallMember0(getInstanceMemberRef, false);
        reWriterWrapper.Cast(traceAgentTypeRef);
        reWriterWrapper.LoadStr(typeNameTextToken);
        reWriterWrapper.LoadStr(methodNameTextToken);
        reWriterWrapper.LoadArgument(0);
        auto argNum = functionInfo.signature.NumberOfArguments();
        reWriterWrapper.CreateArray(objectTypeRef, argNum);
        auto arguments = functionInfo.signature.GetMethodArguments();
        for (unsigned i = 0; i < argNum; i++) {
            reWriterWrapper.BeginLoadValueIntoArray(i);
            reWriterWrapper.LoadArgument(i + 1);
            auto argTypeFlags = arguments[i].GetTypeFlags(elementType);
            if(argTypeFlags & TypeFlagByRef) {
                reWriterWrapper.LoadIND(elementType);
            }
            if (argTypeFlags & TypeFlagBoxedType) {
                auto tok = arguments[i].GetTypeTok(pEmit, corLibAssemblyRef);
                if (tok == mdTokenNil) {
                    return S_OK;
                }
                reWriterWrapper.Box(tok);
            }
            reWriterWrapper.EndLoadValueIntoArray();
        }
        reWriterWrapper.LoadInt32((INT32)function_token);
        reWriterWrapper.CallMember(beforeMemberRef, true);
        reWriterWrapper.Cast(methodTraceTypeRef);
        reWriterWrapper.StLocal(rewriter.cNewLocals - 1);

        ILInstr* pRetInstr = pReWriter->NewILInstr();
        pRetInstr->m_opcode = CEE_RET;
        pReWriter->InsertAfter(pReWriter->GetILList()->m_pPrev, pRetInstr);

        bool isVoidMethod = (retTypeFlags & TypeFlagVoid) > 0;
        auto ret = functionInfo.signature.GetRet();
        bool retIsBoxedType = false;
        mdToken retTypeTok;
        if (!isVoidMethod) {
            retTypeTok = ret.GetTypeTok(pEmit, corLibAssemblyRef);
            if (ret.GetTypeFlags(elementType) & TypeFlagBoxedType) {
                retIsBoxedType = true;
            }
        }
        reWriterWrapper.SetILPosition(pRetInstr);
        reWriterWrapper.StLocal(indexEx);
        ILInstr* pRethrowInstr = reWriterWrapper.Rethrow();

        reWriterWrapper.LoadLocal(indexMethodTrace);
        ILInstr* pNewInstr = pReWriter->NewILInstr();
        pNewInstr->m_opcode = CEE_BRFALSE_S;
        pReWriter->InsertBefore(pRetInstr, pNewInstr);

        reWriterWrapper.LoadLocal(indexMethodTrace);
        reWriterWrapper.LoadLocal(indexRet);
        reWriterWrapper.LoadLocal(indexEx);
        reWriterWrapper.CallMember(endMemberRef, true);

        ILInstr* pEndFinallyInstr = reWriterWrapper.EndFinally();
        pNewInstr->m_pTarget = pEndFinallyInstr;

        if (!isVoidMethod) {
            reWriterWrapper.LoadLocal(indexRet);
            if (retIsBoxedType) {
                reWriterWrapper.UnboxAny(retTypeTok);
            }
            else {
                reWriterWrapper.Cast(retTypeTok);
            }
        }

        for (ILInstr * pInstr = pReWriter->GetILList()->m_pNext;
            pInstr != pReWriter->GetILList();
            pInstr = pInstr->m_pNext) {
            switch (pInstr->m_opcode)
            {
            case CEE_RET:
            {
                if (pInstr != pRetInstr) {
                    if (!isVoidMethod) {
                        reWriterWrapper.SetILPosition(pInstr);
                        if (retIsBoxedType) {
                            reWriterWrapper.Box(retTypeTok);
                        }
                        reWriterWrapper.StLocal(indexRet);
                    }
                    pInstr->m_opcode = CEE_LEAVE_S;
                    pInstr->m_pTarget = pEndFinallyInstr->m_pNext;
                }
                break;
            }
            default:
                break;
            }
        }

        EHClause exClause{};
        exClause.m_Flags = COR_ILEXCEPTION_CLAUSE_NONE;
        exClause.m_pTryBegin = pTryStartInstr;
        exClause.m_pTryEnd = pRethrowInstr->m_pPrev;
        exClause.m_pHandlerBegin = pRethrowInstr->m_pPrev;
        exClause.m_pHandlerEnd = pRethrowInstr;
        exClause.m_ClassToken = exTypeRef;

        EHClause finallyClause{};
        finallyClause.m_Flags = COR_ILEXCEPTION_CLAUSE_FINALLY;
        finallyClause.m_pTryBegin = pTryStartInstr;
        finallyClause.m_pTryEnd = pRethrowInstr->m_pNext;
        finallyClause.m_pHandlerBegin = pRethrowInstr->m_pNext;
        finallyClause.m_pHandlerEnd = pEndFinallyInstr;

        auto m_pEHNew = new EHClause[rewriter.m_nEH + 2];
        for (unsigned i = 0; i < rewriter.m_nEH; i++) {
            m_pEHNew[i] = rewriter.m_pEH[i];
        }

        rewriter.m_nEH += 2;
        m_pEHNew[rewriter.m_nEH - 2] = exClause;
        m_pEHNew[rewriter.m_nEH - 1] = finallyClause;
        rewriter.m_pEH = m_pEHNew;

        hr = rewriter.Export();
        RETURN_OK_IF_FAILED(hr);

        {
            std::lock_guard<std::mutex> guard(mapLock);
            iLRewriteMap[function_token] = true;
        }

        Info("TypeName:{} MethodName:{} IL ReWirte ", ToString(functionInfo.type.name), ToString(functionInfo.name));

        return  S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::JITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::JITCachedFunctionSearchStarted(FunctionID functionId, BOOL *pbUseCachedFunction)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::JITCachedFunctionSearchFinished(FunctionID functionId, COR_PRF_JIT_CACHE result)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::JITFunctionPitched(FunctionID functionId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::JITInlining(FunctionID callerId, FunctionID calleeId, BOOL *pfShouldInline)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ThreadCreated(ThreadID threadId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ThreadDestroyed(ThreadID threadId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ThreadAssignedToOSThread(ThreadID managedThreadId, DWORD osThreadId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientInvocationStarted()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientSendingMessage(GUID *pCookie, BOOL fIsAsync)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientReceivingReply(GUID *pCookie, BOOL fIsAsync)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientInvocationFinished()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerReceivingMessage(GUID *pCookie, BOOL fIsAsync)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerInvocationStarted()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerInvocationReturned()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerSendingReply(GUID *pCookie, BOOL fIsAsync)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::UnmanagedToManagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ManagedToUnmanagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeSuspendStarted(COR_PRF_SUSPEND_REASON suspendReason)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeSuspendFinished()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeSuspendAborted()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeResumeStarted()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeResumeFinished()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeThreadSuspended(ThreadID threadId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeThreadResumed(ThreadID threadId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::MovedReferences(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], ULONG cObjectIDRangeLength[])
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ObjectAllocated(ObjectID objectId, ClassID classId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ObjectsAllocatedByClass(ULONG cClassCount, ClassID classIds[], ULONG cObjects[])
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ObjectReferences(ObjectID objectId, ClassID classId, ULONG cObjectRefs, ObjectID objectRefIds[])
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RootReferences(ULONG cRootRefs, ObjectID rootRefIds[])
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionThrown(ObjectID thrownObjectId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFunctionEnter(FunctionID functionId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFunctionLeave()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFilterEnter(FunctionID functionId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFilterLeave()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchCatcherFound(FunctionID functionId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionOSHandlerEnter(UINT_PTR __unused)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionOSHandlerLeave(UINT_PTR __unused)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFunctionEnter(FunctionID functionId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFunctionLeave()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFinallyEnter(FunctionID functionId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFinallyLeave()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCatcherEnter(FunctionID functionId, ObjectID objectId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCatcherLeave()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::COMClassicVTableCreated(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable, ULONG cSlots)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::COMClassicVTableDestroyed(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCLRCatcherFound()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCLRCatcherExecute()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ThreadNameChanged(ThreadID threadId, ULONG cchName, WCHAR name[])
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::GarbageCollectionStarted(int cGenerations, BOOL generationCollected[], COR_PRF_GC_REASON reason)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::SurvivingReferences(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], ULONG cObjectIDRangeLength[])
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::GarbageCollectionFinished()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::FinalizeableObjectQueued(DWORD finalizerFlags, ObjectID objectID)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::RootReferences2(ULONG cRootRefs, ObjectID rootRefIds[], COR_PRF_GC_ROOT_KIND rootKinds[], COR_PRF_GC_ROOT_FLAGS rootFlags[], UINT_PTR rootIds[])
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::HandleCreated(GCHandleID handleId, ObjectID initialObjectId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::HandleDestroyed(GCHandleID handleId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::InitializeForAttach(IUnknown *pCorProfilerInfoUnk, void *pvClientData, UINT cbClientData)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ProfilerAttachComplete()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ProfilerDetachSucceeded()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ReJITCompilationStarted(FunctionID functionId, ReJITID rejitId, BOOL fIsSafeToBlock)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::GetReJITParameters(ModuleID moduleId, mdMethodDef methodId, ICorProfilerFunctionControl *pFunctionControl)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ReJITCompilationFinished(FunctionID functionId, ReJITID rejitId, HRESULT hrStatus, BOOL fIsSafeToBlock)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ReJITError(ModuleID moduleId, mdMethodDef methodId, FunctionID functionId, HRESULT hrStatus)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::MovedReferences2(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], SIZE_T cObjectIDRangeLength[])
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::SurvivingReferences2(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], SIZE_T cObjectIDRangeLength[])
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ConditionalWeakTableElementReferences(ULONG cRootRefs, ObjectID keyRefIds[], ObjectID valueRefIds[], GCHandleID rootIds[])
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::GetAssemblyReferences(const WCHAR *wszAssemblyPath, ICorProfilerAssemblyReferenceProvider *pAsmRefProvider)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ModuleInMemorySymbolsUpdated(ModuleID moduleId)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::DynamicMethodJITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock, LPCBYTE ilHeader, ULONG cbILHeader)
    { 
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::DynamicMethodJITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock)
    {
        return S_OK;
    }
}
