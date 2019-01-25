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
        TraceMethod(WSTRING methodName, WSTRING paramsName) : methodName(methodName), paramsName(paramsName) {}
    };

    struct TraceAssembly
    {
        WSTRING assemblyName;
        WSTRING className;
        std::vector<TraceMethod> methods;
    };

    std::vector<TraceAssembly> LoadTraceAssemblies(const WSTRING& traceHomePath);
}

#endif
