#ifndef CLR_PROFILER_CLRHELPER_H_
#define CLR_PROFILER_CLRHELPER_H_

#include <functional>
#include <vector>
#include "string.h"  // NOLINT
#include "util.h"
#include "CComPtr.h"
#include <corprof.h>

namespace trace {

    const size_t kNameMaxSize = 1024;
    const ULONG kEnumeratorMax = 256;
    const auto kProfilerAssemblyName = "ClrProfiler.Trace"_W;
    const auto kTraceAgentTypeName = "ClrProfiler.Trace.TraceAgent"_W;
    const auto kGetInstanceMethodName = "GetInstance"_W;
    const auto kBeforeMethodName = "BeforeMethod"_W;
    const auto kEndMethodName = "EndMethod"_W;
    const auto kMethodTraceTypeName = "ClrProfiler.Trace.MethodTrace"_W;

    const auto kAssemblyTypeName = "System.Reflection.Assembly"_W;
    const auto kAssemblyLoadMethodName = "LoadFrom"_W;
    const auto kAssemblyCustomLoadMethodName = "CustomLoadFrom"_W;

    const auto kClrProfilerHomeEnv = "CLRPROFILER_HOME"_W;

    const auto kClrProfilerDllName = "ClrProfiler.Trace.dll"_W;

    const auto SystemBoolean = "System.Boolean"_W;
    const auto SystemChar = "System.Char"_W;
    const auto SystemByte = "System.Byte"_W;
    const auto SystemSByte = "System.SByte"_W;
    const auto SystemUInt16 = "System.UInt16"_W;
    const auto SystemInt16 = "System.Int16"_W;
    const auto SystemInt32 = "System.Int32"_W;
    const auto SystemUInt32 = "System.UInt32"_W;
    const auto SystemInt64 = "System.Int64"_W;
    const auto SystemUInt64 = "System.UInt64"_W;
    const auto SystemSingle = "System.Single"_W;
    const auto SystemDouble = "System.Double"_W;
    const auto SystemIntPtr = "System.IntPtr"_W;
    const auto SystemUIntPtr = "System.UIntPtr"_W;
    const auto SystemString = "System.String"_W;
    const auto SystemObject = "System.Object"_W;
    const auto SystemException = "System.Exception"_W;

    template <typename T>
    class EnumeratorIterator;

    template <typename T>
    class Enumerator {
    private:
        const std::function<HRESULT(HCORENUM*, T[], ULONG, ULONG*)> callback_;
        const std::function<void(HCORENUM)> close_;
        mutable HCORENUM ptr_;

    public:
        Enumerator(std::function<HRESULT(HCORENUM*, T[], ULONG, ULONG*)> callback,
            std::function<void(HCORENUM)> close)
            : callback_(callback), close_(close), ptr_(nullptr) {}

        Enumerator(const Enumerator& other) = default;

        Enumerator& operator=(const Enumerator& other) = default;

        ~Enumerator() { close_(ptr_); }

        EnumeratorIterator<T> begin() const {
            return EnumeratorIterator<T>(this, S_OK);
        }

        EnumeratorIterator<T> end() const {
            return EnumeratorIterator<T>(this, S_FALSE);
        }

        HRESULT Next(T arr[], ULONG max, ULONG* cnt) const {
            return callback_(&ptr_, arr, max, cnt);
        }
    };

    template <typename T>
    class EnumeratorIterator {
    private:
        const Enumerator<T>* enumerator_;
        HRESULT status_ = S_FALSE;
        T arr_[kEnumeratorMax]{};
        ULONG idx_ = 0;
        ULONG sz_ = 0;

    public:
        EnumeratorIterator(const Enumerator<T>* enumerator, HRESULT status)
            : enumerator_(enumerator) {
            if (status == S_OK) {
                status_ = enumerator_->Next(arr_, kEnumeratorMax, &sz_);
                if (status_ == S_OK && sz_ == 0) {
                    status_ = S_FALSE;
                }
            }
            else {
                status_ = status;
            }
        }

        bool operator!=(EnumeratorIterator const& other) const {
            return enumerator_ != other.enumerator_ ||
                (status_ == S_OK) != (other.status_ == S_OK);
        }

        T const& operator*() const { return arr_[idx_]; }

        EnumeratorIterator<T>& operator++() {
            if (idx_ < sz_ - 1) {
                idx_++;
            }
            else {
                idx_ = 0;
                status_ = enumerator_->Next(arr_, kEnumeratorMax, &sz_);
                if (status_ == S_OK && sz_ == 0) {
                    status_ = S_FALSE;
                }
            }
            return *this;
        }
    };

    static Enumerator<mdTypeDef> EnumTypeDefs(
        const CComPtr<IMetaDataImport2>& metadata_import) {
        return Enumerator<mdTypeDef>(
            [metadata_import](HCORENUM* ptr, mdTypeDef arr[], ULONG max,
                ULONG* cnt) -> HRESULT {
            return metadata_import->EnumTypeDefs(ptr, arr, max, cnt);
        },
            [metadata_import](HCORENUM ptr) -> void {
            metadata_import->CloseEnum(ptr);
        });
    }

    static Enumerator<mdTypeRef> EnumTypeRefs(
        const CComPtr<IMetaDataImport2>& metadata_import) {
        return Enumerator<mdTypeRef>(
            [metadata_import](HCORENUM* ptr, mdTypeRef arr[], ULONG max,
                ULONG* cnt) -> HRESULT {
            return metadata_import->EnumTypeRefs(ptr, arr, max, cnt);
        },
            [metadata_import](HCORENUM ptr) -> void {
            metadata_import->CloseEnum(ptr);
        });
    }

    static Enumerator<mdMethodDef> EnumMethods(
        const CComPtr<IMetaDataImport2>& metadata_import,
        const mdToken& parent_token) {
        return Enumerator<mdMethodDef>(
            [metadata_import, parent_token](HCORENUM* ptr, mdMethodDef arr[],
                ULONG max, ULONG* cnt) -> HRESULT {
            return metadata_import->EnumMethods(ptr, parent_token, arr, max, cnt);
        },
            [metadata_import](HCORENUM ptr) -> void {
            metadata_import->CloseEnum(ptr);
        });
    }

    static Enumerator<mdMemberRef> EnumMemberRefs(
        const CComPtr<IMetaDataImport2>& metadata_import,
        const mdToken& parent_token) {
        return Enumerator<mdMemberRef>(
            [metadata_import, parent_token](HCORENUM* ptr, mdMemberRef arr[],
                ULONG max, ULONG* cnt) -> HRESULT {
            return metadata_import->EnumMemberRefs(ptr, parent_token, arr, max,
                cnt);
        },
            [metadata_import](HCORENUM ptr) -> void {
            metadata_import->CloseEnum(ptr);
        });
    }

    static Enumerator<mdModuleRef> EnumModuleRefs(
        const CComPtr<IMetaDataImport2>& metadata_import) {
        return Enumerator<mdModuleRef>(
            [metadata_import](HCORENUM* ptr, mdModuleRef arr[], ULONG max,
                ULONG* cnt) -> HRESULT {
            return metadata_import->EnumModuleRefs(ptr, arr, max, cnt);
        },
            [metadata_import](HCORENUM ptr) -> void {
            metadata_import->CloseEnum(ptr);
        });
    }

    static Enumerator<mdAssemblyRef> EnumAssemblyRefs(
        const CComPtr<IMetaDataAssemblyImport>& assembly_import) {
        return Enumerator<mdAssemblyRef>(
            [assembly_import](HCORENUM* ptr, mdAssemblyRef arr[], ULONG max,
                ULONG* cnt) -> HRESULT {
            return assembly_import->EnumAssemblyRefs(ptr, arr, max, cnt);
        },
            [assembly_import](HCORENUM ptr) -> void {
            assembly_import->CloseEnum(ptr);
        });
    }

    static Enumerator<mdParamDef> EnumParams(
        const CComPtr<IMetaDataImport2>& metadata_import, const mdMethodDef& mb) {
        return Enumerator<mdParamDef>(
            [metadata_import, mb](HCORENUM* ptr, mdParamDef arr[], ULONG max,
                ULONG* cnt) -> HRESULT {
            return metadata_import->EnumParams(ptr,mb, arr, max, cnt);
        },
            [metadata_import](HCORENUM ptr) -> void {
            metadata_import->CloseEnum(ptr);
        });
    }

    static Enumerator<mdGenericParam> EnumGenericParams(
        const CComPtr<IMetaDataImport2>& metadata_import, const mdMethodDef& mb) {
        return Enumerator<mdGenericParam>(
            [metadata_import, mb](HCORENUM* ptr, mdGenericParam arr[], ULONG max,
                ULONG* cnt) -> HRESULT {
            return metadata_import->EnumGenericParams(ptr, mb, arr, max, cnt);
        },
            [metadata_import](HCORENUM ptr) -> void {
            metadata_import->CloseEnum(ptr);
        });
    }

    static Enumerator<mdGenericParamConstraint> EnumGenericParamConstraints(
        const CComPtr<IMetaDataImport2>& metadata_import, const mdGenericParam& mb) {
        return Enumerator<mdGenericParamConstraint>(
            [metadata_import, mb](HCORENUM* ptr, mdGenericParamConstraint arr[], ULONG max,
                ULONG* cnt) -> HRESULT {
            return metadata_import->EnumGenericParamConstraints(ptr, mb, arr, max, cnt);
        },
            [metadata_import](HCORENUM ptr) -> void {
            metadata_import->CloseEnum(ptr);
        });
    }

    static Enumerator<mdToken> EnumMembersWithName(
        const CComPtr<IMetaDataImport2>& metadata_import,
        const mdToken& parent_token,
        LPCWSTR szName) {
        return Enumerator<mdToken>(
            [metadata_import, parent_token, szName](HCORENUM* ptr, mdMethodDef arr[],
                ULONG max, ULONG* cnt) -> HRESULT {
            return metadata_import->EnumMembersWithName(ptr, parent_token, szName, arr, max, cnt);
        },
            [metadata_import](HCORENUM ptr) -> void {
            metadata_import->CloseEnum(ptr);
        });
    }

    struct AssemblyProperty {
        const void  *ppbPublicKey;
        ULONG       pcbPublicKey;
        ULONG       pulHashAlgId;
        ASSEMBLYMETADATA pMetaData{};
        WSTRING     szName;
        DWORD assemblyFlags = 0;

        AssemblyProperty() : ppbPublicKey(nullptr), pcbPublicKey(0), pulHashAlgId(0), szName(""_W)
        {
        }
    };

    struct AssemblyInfo {
        const AssemblyID id;
        const WSTRING name;

        AssemblyInfo() : id(0), name(""_W) {}
        AssemblyInfo(AssemblyID id, WSTRING name) : id(id), name(name) {}

        bool is_valid() const { return id != 0; }
    };

    struct ModuleInfo {
        const ModuleID id;
        const WSTRING path;
        const AssemblyInfo assembly;
        const DWORD flags;
        const LPCBYTE baseLoadAddress;

        ModuleInfo() : id(0), path(""_W), assembly({}), flags(0), baseLoadAddress(nullptr){}
        ModuleInfo(ModuleID id, WSTRING path, AssemblyInfo assembly, DWORD flags, LPCBYTE baseLoadAddress)
            : id(id), path(path), assembly(assembly), flags(flags), baseLoadAddress(baseLoadAddress) {}

        bool IsValid() const { return id != 0; }

        bool IsWindowsRuntime() const {
            return ((flags & COR_PRF_MODULE_WINDOWS_RUNTIME) != 0);
        }

        mdToken GetEntryPointToken() const {
            const auto ntHeaders = (IMAGE_NT_HEADERS*)(baseLoadAddress + VAL32(((IMAGE_DOS_HEADER*)baseLoadAddress)->e_lfanew));
            const auto directoryEntry = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COMHEADER];
            const auto corHeader = (IMAGE_COR20_HEADER*)(baseLoadAddress + VAL32(directoryEntry.VirtualAddress));
            return corHeader->EntryPointToken;
        }
    };

    struct TypeInfo {
        const mdToken id;
        const WSTRING name;

        TypeInfo() : id(0), name(""_W) {}
        TypeInfo(mdToken id, WSTRING name) : id(id), name(name) {}

        bool IsValid() const { return id != 0; }
    };

    struct MethodArgument {
        ULONG offset;
        ULONG length;
        PCCOR_SIGNATURE pbBase;
        mdToken GetTypeTok(CComPtr<IMetaDataEmit2>& pEmit, mdAssemblyRef corLibRef) const;
        WSTRING GetTypeTokName(CComPtr<IMetaDataImport2>& pImport) const;
        bool IsBoxedType() const;
    private:
        WSTRING GetTypeTokName(PCCOR_SIGNATURE& pbCur, CComPtr<IMetaDataImport2>& pImport) const;
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
        MethodSignature(): pbBase(nullptr), len(0){}
        MethodSignature(PCCOR_SIGNATURE pb, unsigned cbBuffer) {
            pbBase = pb;
            len = cbBuffer;
        };
        ULONG NumberOfTypeArguments() const { return numberOfTypeArguments; }
        ULONG NumberOfArguments() const { return numberOfArguments; }
        WSTRING str() const { return HexStr(pbBase, len); }
        bool IsVoidMethod() const {
            const auto pbCur = &ret.pbBase[ret.offset];
            return *pbCur == ELEMENT_TYPE_VOID;
        }
        MethodArgument GetRet() const { return  ret; }
        std::vector<MethodArgument> GetMethodArguments() const { return params; }
        HRESULT TryParse();
        bool operator ==(const MethodSignature& other) const {
            return memcmp(pbBase, other.pbBase, len);
        }
        CorCallingConvention CallingConvention() const {
            return CorCallingConvention(len == 0 ? 0 : pbBase[0]);
        }
        bool IsEmpty() const  {
            return len == 0;
        }
    };

    struct FunctionInfo {
        const mdToken id;
        const WSTRING name;
        const TypeInfo type;
        MethodSignature signature;

        FunctionInfo() : id(0), name(""_W), type({}), signature({}) {}
        FunctionInfo(mdToken id, WSTRING name, TypeInfo type,
            MethodSignature signature)
            : id(id), name(name), type(type), signature(signature) {}

        bool IsValid() const { return id != 0; }
    };

    WSTRING GetAssemblyName(const CComPtr<IMetaDataAssemblyImport>& assembly_import);

    WSTRING GetAssemblyName(const CComPtr<IMetaDataAssemblyImport>& assembly_import,
        const mdAssemblyRef& assembly_ref);

    mdAssemblyRef FindAssemblyRef(
        const CComPtr<IMetaDataAssemblyImport>& assembly_import,
        const WSTRING& assembly_name);

    AssemblyInfo GetAssemblyInfo(ICorProfilerInfo3* info,
        const AssemblyID& assembly_id);

    ModuleInfo GetModuleInfo(ICorProfilerInfo3* info, const ModuleID& module_id);

    TypeInfo GetTypeInfo(const CComPtr<IMetaDataImport2>& metadata_import,
        const mdToken& token);

    mdAssemblyRef GetCorLibAssemblyRef(CComPtr<IUnknown>& metadata_interfaces,
        AssemblyProperty assemblyProperty);

    FunctionInfo GetFunctionInfo(const CComPtr<IMetaDataImport2>& metadata_import,
        const mdToken& token);

    HRESULT GetProfilerAssemblyRef(CComPtr<IUnknown>& metadata_interfaces, 
        mdAssemblyRef& assemblyRef);
}

#endif  // CLR_PROFILER_CLRHELPER_H_
