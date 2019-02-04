#ifndef CLR_PROFILER_TRACECONFIG_LOADER_H_
#define CLR_PROFILER_TRACECONFIG_LOADER_H_

#include <string>
#include <vector>
#include "string.h"   // NOLINT

namespace trace {

    struct TraceMethod
    {
         WSTRING methodName;
        WSTRING paramsName;
        TraceMethod() : methodName(""_W), paramsName(""_W) {}
        TraceMethod(WSTRING methodName, WSTRING paramsName) : methodName(methodName), paramsName(paramsName) {}    };

    struct TraceAssembly
    {
        WSTRING assemblyName;
        WSTRING className;
        std::vector<TraceMethod> methods;
    };

    struct Version {
        unsigned short major = 1;
        unsigned short minor = 0;
        unsigned short build = 0;
        unsigned short revision = 0;
        Version() {}
        Version(std::vector<WSTRING> vector)
        {
            major = GetIndexNum(vector, 0);
            minor = GetIndexNum(vector, 1);
            build = GetIndexNum(vector, 2);
            revision = GetIndexNum(vector, 3);
        }

    private:
        unsigned short GetIndexNum(std::vector<WSTRING> vector, unsigned index) const
        {
            const auto size = vector.size();
            if (size > index)
                return (unsigned short)strtol(ToString(vector[index]).c_str(), NULL, 10);
            return 0;
        }
    };

    struct ManagedAssembly
    {
        std::vector<BYTE> publicKey;
        ASSEMBLYMETADATA assemblyMetaData{};
    };

    struct TraceConfig
    {
        std::vector<TraceAssembly> traceAssemblies;
        ManagedAssembly managedAssembly{};
    };

    TraceConfig LoadTraceConfig(const WSTRING& traceHomePath);

}

#endif
