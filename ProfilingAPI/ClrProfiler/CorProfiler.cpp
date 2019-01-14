// Copyright (c) .NET Foundation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "CorProfiler.h"
#include "corhlpr.h"
#include "CComPtr.h"
#include "logging.h"  // NOLINT
#include "macros.h"
#include "clr_helpers.h"
#include "il_rewriter.h"
#include <string>
#include <vector>
#include <iostream>
#include <cassert>

namespace trace {

    static void STDMETHODCALLTYPE Enter(FunctionID functionId)
    {
        //printf("\r\nEnter %" UINT_PTR_FORMAT "", (UINT64)functionId);
    }

    static void STDMETHODCALLTYPE Leave(FunctionID functionId)
    {
        //printf("\r\nLeave %" UINT_PTR_FORMAT "", (UINT64)functionId);
    }

    COR_SIGNATURE enterLeaveMethodSignature[] = { IMAGE_CEE_CS_CALLCONV_STDCALL, 0x01, ELEMENT_TYPE_VOID, ELEMENT_TYPE_I };

    void(STDMETHODCALLTYPE *EnterMethodAddress)(FunctionID) = &Enter;
    void(STDMETHODCALLTYPE *LeaveMethodAddress)(FunctionID) = &Leave;

    CorProfiler::CorProfiler() : refCount(0), corProfilerInfo(nullptr)
    {
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
        HRESULT queryInterfaceResult = pICorProfilerInfoUnk->QueryInterface(__uuidof(ICorProfilerInfo8), reinterpret_cast<void **>(&this->corProfilerInfo));

        if (FAILED(queryInterfaceResult))
        {
            return E_FAIL;
        }

        DWORD eventMask = COR_PRF_MONITOR_JIT_COMPILATION |
            COR_PRF_DISABLE_TRANSPARENCY_CHECKS_UNDER_FULL_TRUST | /* helps the case where this profiler is used on Full CLR */
            COR_PRF_DISABLE_INLINING |
            COR_PRF_MONITOR_MODULE_LOADS |
            COR_PRF_DISABLE_ALL_NGEN_IMAGES;

        auto hr = this->corProfilerInfo->SetEventMask(eventMask);

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

    HRESULT getConstraintTypes(IMetaDataImport2* metadata_import, const unsigned mdGenericParam, mdToken constraintTypes[])
    {
        std::vector<mdToken> ptkConstraintTypes;
        HCORENUM hEnum = HCORENUM();
        ULONG gpcCount = 0ul;
        const ULONG maxGpcCount = 8ul;

        do
        {
            mdGenericParamConstraint mdGenericParamConstraints[maxGpcCount];
            HRESULT hr = metadata_import->EnumGenericParamConstraints(&hEnum,
                                                                      mdGenericParam,
                                                                      mdGenericParamConstraints,
                                                                      maxGpcCount,
                                                                      &gpcCount);
            IfFailRet(hr);

            for (auto k = 0u; k < gpcCount; ++k)
            {
                mdToken ptkConstraintType;
                hr = metadata_import->GetGenericParamConstraintProps(mdGenericParamConstraints[k],
                                                                     NULL,
                                                                     &ptkConstraintType);
                IfFailRet(hr);
                ptkConstraintTypes.push_back(ptkConstraintType);
            }
        } while (0 < gpcCount);
        if (hEnum)
        {
            metadata_import->CloseEnum(hEnum);
        }
        constraintTypes = ptkConstraintTypes.data();
        return S_OK;
    }

    HRESULT copyGenericParams(IMetaDataImport2* metadata_import, IMetaDataEmit2* pEmit, mdMethodDef mdProb, mdMethodDef mdWrapper)
    {
        HCORENUM hEnum = HCORENUM();
        ULONG count = 0ul;
        const ULONG maxCount = 16;
        mdGenericParam mdGenericParams[maxCount];
        do
        {
            HRESULT hr = metadata_import->EnumGenericParams(&hEnum, mdProb, mdGenericParams, maxCount, &count);
            IfFailRet(hr);
            for (auto j = 0u; j < count; ++j)
            {
                const auto kmdGenericParam = mdGenericParams[j];

                const size_t knNameMaxSize = 1024;
                WCHAR paramName[knNameMaxSize];
                ULONG        pulParamSeq;
                DWORD        pdwParamFlags;
                mdToken      ptOwner;
                DWORD       reserved;
                ULONG        pchName;
                hr = metadata_import->GetGenericParamProps(kmdGenericParam,
                    &pulParamSeq,
                    &pdwParamFlags,
                    &ptOwner,
                    &reserved,
                    paramName,
                    sizeof(knNameMaxSize),
                    &pchName);

                IfFailRet(hr);

                auto paramNameStr = WSTRING(paramName);

                mdToken* constraintTypes = NULL;
                hr = getConstraintTypes(metadata_import, kmdGenericParam, constraintTypes);
                IfFailRet(hr);

                mdGenericParam pgp;
                hr = pEmit->DefineGenericParam(mdWrapper,
                    pulParamSeq,
                    pdwParamFlags,
                    paramNameStr.data(),
                    0,
                    constraintTypes,
                    &pgp);
                
                IfFailRet(hr);
            }
        } while (0 < count);
        if (hEnum)
        {
            metadata_import->CloseEnum(hEnum);
        }

        return S_OK;
    }

    HRESULT copyParams(IMetaDataImport2* metadata_import, IMetaDataEmit2* pEmit, mdMethodDef mdProb, mdMethodDef mdWrapper)
    {
        HCORENUM hEnum = HCORENUM();
        ULONG count = 0ul;
        const ULONG maxCount = 16;
        mdParamDef mdParamDefs[maxCount];
        do
        {
            HRESULT hr = metadata_import->EnumParams(&hEnum, mdProb, mdParamDefs, maxCount, &count);
            IfFailRet(hr);

            for (auto j = 0u; j < count; ++j)
            {
                const auto kmdParamDef = mdParamDefs[j];

                const size_t knNameMaxSize = 1024;
                ULONG pulParamSeq;
                WCHAR paramName[knNameMaxSize];
                DWORD       pdwAttr;
                DWORD       pdwCPlusTypeFlag;
                UVCP_CONSTANT ppValue;
                ULONG       pcchValue;
                hr = metadata_import->GetParamProps(kmdParamDef,
                    NULL,
                    &pulParamSeq,
                    paramName,
                    sizeof(paramName),
                    NULL,
                    &pdwAttr,
                    &pdwCPlusTypeFlag,
                    &ppValue,
                    &pcchValue);

                IfFailRet(hr);
                auto paramNameStr = WSTRING(paramName);

                mdParamDef ppd;
                hr = pEmit->DefineParam(mdWrapper,
                    pulParamSeq,
                    paramNameStr.data(),
                    pdwAttr,
                    pdwCPlusTypeFlag,
                    ppValue,
                    pcchValue,
                    &ppd);

                IfFailRet(hr);
            }                       
        } while (0 < count);
        if(hEnum)
        {
            metadata_import->CloseEnum(hEnum);
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::ModuleLoadFinished(ModuleID moduleId, HRESULT hrStatus)
    {
        const DWORD module_path_size = 260;
        WCHAR module_path[module_path_size]{};
        DWORD module_path_len = 0;
        LPCBYTE base_load_address;
        AssemblyID assembly_id = 0;
        DWORD module_flags = 0;
        HRESULT hr = corProfilerInfo->GetModuleInfo2(
            moduleId, &base_load_address, module_path_size, &module_path_len,
            module_path, &assembly_id, &module_flags);
        RETURN_OK_IF_FAILED(hr);
        if ((module_flags & COR_PRF_MODULE_WINDOWS_RUNTIME) != 0 || module_path_len == 0) {
            return S_OK;
        }

        const size_t kNameMaxSize = 1024;
        WCHAR name[kNameMaxSize];
        DWORD name_len = 0;
        hr = corProfilerInfo->GetAssemblyInfo(assembly_id, kNameMaxSize, &name_len, name,
            NULL, NULL);
        RETURN_OK_IF_FAILED(hr);

        if (WSTRING(name) == "StackExchange.Redis"_W)
        {
            CComPtr<IUnknown> metadata_interfaces;
            hr = corProfilerInfo->GetModuleMetaData(moduleId, ofRead | ofWrite,
                IID_IMetaDataImport2,
                &metadata_interfaces);
            RETURN_OK_IF_FAILED(hr);

            CComPtr<IMetaDataImport2> metadata_import;
            hr = metadata_interfaces->QueryInterface(IID_IMetaDataImport, (LPVOID *)&metadata_import);
            RETURN_OK_IF_FAILED(hr);

            CComPtr<IMetaDataAssemblyEmit> pAssemblyEmit;
            hr = metadata_interfaces->QueryInterface(IID_IMetaDataAssemblyEmit, (LPVOID *)&pAssemblyEmit);
            RETURN_OK_IF_FAILED(hr);

            CComPtr<IMetaDataEmit2> pEmit;
            hr = metadata_interfaces->QueryInterface(IID_IMetaDataEmit, (LPVOID *)&pEmit);
            RETURN_OK_IF_FAILED(hr);

            mdModule module;
            hr = metadata_import->GetModuleFromScope(&module);
            RETURN_OK_IF_FAILED(hr);

            //1. .net framework gac or net core DOTNET_ADDITIONAL_DEPS=%PROGRAMFILES%\dotnet\x64\additionalDeps\Datadog.Trace.ClrProfiler.Managed
            //2. just proj ref
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

            mdAssemblyRef assemblyRef = NULL;
            hr = pAssemblyEmit->DefineAssemblyRef(
                (void *)rgbPublicKeyToken,
                sizeof(rgbPublicKeyToken),
                L"Datadog.Trace.ClrProfiler.Managed",
                &assemblyMetaData,
                NULL,                   // hash blob
                NULL,                   // cb of hash blob
                0,                      // flags
                &assemblyRef);
            RETURN_OK_IF_FAILED(hr);

            const LPCWSTR wszTypeToReference = L"Datadog.Trace.ClrProfiler.Integrations.StackExchange.Redis.ConnectionMultiplexer";
            mdTypeRef typeRef = mdTokenNil;
            hr = pEmit->DefineTypeRefByName(
                assemblyRef,
                wszTypeToReference,
                &typeRef);
            RETURN_OK_IF_FAILED(hr);

            const LPCWSTR mdProbeName = L"ExecuteSyncImpl";
            std::vector<BYTE> sigFunctionProbe = { 0x10,0x01,0x04,0x1E,0x00,0x1C,0x1C,0x1C,0x1C };
            
            mdMemberRef pmr;
            hr = pEmit->DefineMemberRef(
                typeRef,
                mdProbeName,
                sigFunctionProbe.data(),
                (DWORD)sigFunctionProbe.size(),
                &pmr);
            
            RETURN_OK_IF_FAILED(hr);

            const LPCWSTR typeNameToProb = L"StackExchange.Redis.ConnectionMultiplexer";
            mdTypeDef typeToProb;
            hr = metadata_import->FindTypeDefByName(
                typeNameToProb,
                mdTokenNil,
                &typeToProb);
            RETURN_OK_IF_FAILED(hr);

            LPCWSTR smdProbeName = L"ExecuteSyncImpl";
            mdMethodDef mdProb;
            std::vector<BYTE> sigFunctionProbe2;
            hr = metadata_import->FindMember(
                typeToProb,
                smdProbeName,
                sigFunctionProbe2.data(),
                (DWORD)sigFunctionProbe2.size(),
                &mdProb);
            RETURN_OK_IF_FAILED(hr);

            DWORD       pdwAttr;
            PCCOR_SIGNATURE ppvSigBlob;
            ULONG       pcbSigBlob;
            ULONG       pulCodeRVA;
            DWORD       pdwImplFlags;
            hr = metadata_import->GetMethodProps(
                mdProb,
                NULL,	   // Put method's class here. 
                NULL,		   // Put method's name here.  
                0,			       // Size of szMethod buffer in wide chars.   
                NULL,		   // Put actual size here 
                &pdwAttr,		   // Put flags here.  
                &ppvSigBlob,       // [OUT] point to the blob value of meta data   
                &pcbSigBlob,	   // [OUT] actual size of signature blob  
                &pulCodeRVA,
                &pdwImplFlags);
            RETURN_OK_IF_FAILED(hr);

            auto signature = MethodSignature(ppvSigBlob, pcbSigBlob);
            auto numberOfArguments = signature.NumberOfArguments();
            auto numberOfTypeArguments = signature.NumberOfTypeArguments();
            if(numberOfArguments < 16 && numberOfTypeArguments < 16)
            {
                const WSTRING prefixStr = "Trace_"_W;
                WSTRING mdProbeWrapperName = prefixStr + WSTRING(smdProbeName);
                mdMethodDef mdWrapper;
                hr = pEmit->DefineMethod(
                    typeToProb,
                    mdProbeWrapperName.data(),
                    pdwAttr,
                    ppvSigBlob,
                    pcbSigBlob,
                    pulCodeRVA,
                    pdwImplFlags,
                    &mdWrapper);
                RETURN_OK_IF_FAILED(hr);

                if (numberOfTypeArguments > 0)
                {
                    hr = copyGenericParams(metadata_import, pEmit, mdProb, mdWrapper);
                    RETURN_OK_IF_FAILED(hr);
                }

                if (numberOfArguments > 0)
                {
                    hr = copyParams(metadata_import, pEmit, mdProb, mdWrapper);
                    RETURN_OK_IF_FAILED(hr);
                }

                LPCBYTE pMethodBytes;
                ULONG pMethodSize;
                hr = corProfilerInfo->GetILFunctionBody(moduleId, mdProb, &pMethodBytes, &pMethodSize);
                RETURN_OK_IF_FAILED(hr);

                IMethodMalloc* pIMethodMalloc;
                hr = corProfilerInfo->GetILFunctionBodyAllocator(moduleId, &pIMethodMalloc);
                RETURN_OK_IF_FAILED(hr);

                LPBYTE pNewMethodBytes = (LPBYTE)pIMethodMalloc->Alloc(pMethodSize);
                memcpy(pNewMethodBytes, pMethodBytes, pMethodSize);
                hr = corProfilerInfo->SetILFunctionBody(moduleId, mdWrapper, pNewMethodBytes);
                RETURN_OK_IF_FAILED(hr);

                //ldarg.0 call mdtoken ret
                unsigned codeSizeNew = 1 + 1 + 4 + 1;
                for (unsigned i = 1; i <= numberOfArguments; i++)
                {
                    if (i < 4)
                    {
                        codeSizeNew += 1;
                    }
                    else
                    {
                        //ldarg.s 1
                        codeSizeNew += 2;
                    }
                }
                LPBYTE pwMethodBytes = (LPBYTE)pIMethodMalloc->Alloc(codeSizeNew + 1);
                unsigned offset = 0;
                pwMethodBytes[offset++] = (BYTE)(CorILMethod_TinyFormat | (codeSizeNew << 2));
                pwMethodBytes[offset++] = (BYTE)CEE_LDARG_0;
                
                for (unsigned i = 1; i <= numberOfArguments; i++)
                {
                    if (i == 1)
                    {
                        pwMethodBytes[offset++] = (BYTE)CEE_LDARG_1;
                    }
                    else if (i == 2)
                    {
                        pwMethodBytes[offset++] = (BYTE)CEE_LDARG_2;
                    }
                    else if (i == 3)
                    {
                        pwMethodBytes[offset++] = (BYTE)CEE_LDARG_3;
                    }
                    else
                    {
                        pwMethodBytes[offset++] = (BYTE)CEE_LDARG_S;
                        pwMethodBytes[offset++] = (BYTE)i;
                    }
                }
                pwMethodBytes[offset++] = (BYTE)CEE_CALL;

                mdToken pmi = pmr;
                if(numberOfTypeArguments >0)
                {
                    ULONG raw_signature_len = (ULONG)numberOfTypeArguments * 2 + 2;
                    BYTE* raw_signature = new BYTE[raw_signature_len];
                    ULONG goffset = 0;
                    raw_signature[goffset++] = (BYTE)IMAGE_CEE_CS_CALLCONV_GENERICINST;
                    raw_signature[goffset++] = (BYTE)numberOfTypeArguments;
                    for (ULONG i = 0; i < numberOfTypeArguments; i++)
                    {
                        raw_signature[goffset++] = (BYTE)ELEMENT_TYPE_MVAR;
                        raw_signature[goffset++] = (BYTE)0x00;
                    }
                    Info(HexStr(raw_signature, raw_signature_len));
                    mdMethodSpec kpmi;
                    hr = pEmit->DefineMethodSpec(pmr, raw_signature, raw_signature_len, &kpmi);
                    RETURN_OK_IF_FAILED(hr);
                    pmi = kpmi;
                }

                *(UNALIGNED INT32*)&(pwMethodBytes[offset]) = pmi;
                offset += 4;

                pwMethodBytes[offset++] = (BYTE)CEE_RET;


                Info(offset);
                Info(HexStr(pwMethodBytes, offset));

                hr = corProfilerInfo->SetILFunctionBody(moduleId, mdProb, pwMethodBytes);
                RETURN_OK_IF_FAILED(hr);

                pIMethodMalloc->Release();
            }
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

    HRESULT STDMETHODCALLTYPE CorProfiler::JITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock)
    {
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
        //printf("\r\nDynamic Function JIT Compilation Started. %" UINT_PTR_FORMAT "", (UINT64)functionId);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CorProfiler::DynamicMethodJITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock)
    {
        //printf("\r\nDynamic Function JIT Compilation Finished. %" UINT_PTR_FORMAT "", (UINT64)functionId);
        return S_OK;
    }
}
