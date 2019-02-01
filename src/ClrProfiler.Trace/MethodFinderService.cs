using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using ClrProfiler.Trace.Attributes;
using OpenTracing;

namespace ClrProfiler.Trace
{
    public class MethodFinderService
    {
        private static readonly ConcurrentDictionary<uint, FunctionInfoCache> FunctionInfosCache =
            new ConcurrentDictionary<uint, FunctionInfoCache>();

        private readonly ConcurrentDictionary<string, AssemblyInfoCache> _assemblies = 
            new ConcurrentDictionary<string, AssemblyInfoCache>();

        private readonly ITracer _tracer;

        public MethodFinderService(ITracer tracer)
        {
            _tracer = tracer;

            PrepareAssemblyInfoCache();
        }

        private void PrepareAssemblyInfoCache()
        {
            var home = Environment.GetEnvironmentVariable("CLRPROFILER_HOME");
            if (!string.IsNullOrEmpty(home))
            {
                foreach (string dllPath in Directory.GetFiles(home, "*.dll"))
                {
                    try
                    {
                        var assembly = Assembly.LoadFile(dllPath);
                        var attr = assembly.GetCustomAttribute<TargetAssemblyAttribute>();
                        if (attr != null)
                        {
                            foreach (var name in attr.Names)
                            {
                                _assemblies.TryAdd(name, new AssemblyInfoCache()
                                {
                                    Assembly = assembly
                                });
                            }
                        }
                    }
                    catch (BadImageFormatException ex)
                    {
                        System.Diagnostics.Trace.WriteLine(ex);
                    }
                }
            }
        }

        public EndMethodDelegate BeforeWrappedMethod(object invocationTarget,
            object[] methodArguments,
            uint functionToken)
        {
            if (invocationTarget == null)
            {
                throw new ArgumentException(nameof(invocationTarget));
            }

            var traceMethodInfo = new TraceMethodInfo
            {
                InvocationTarget = invocationTarget,
                MethodArguments = methodArguments
            };

            var functionInfo = GetFunctionInfoFromCache(functionToken, traceMethodInfo);
            traceMethodInfo.MethodBase = functionInfo.MethodBase;

            if (functionInfo.Wrapper == null)
            {
                PrepareMethodWrapper(functionInfo, traceMethodInfo);
            }
            return functionInfo.Wrapper?.BeforeWrappedMethod(traceMethodInfo);
        }

        private void PrepareMethodWrapper(FunctionInfoCache functionInfo, TraceMethodInfo traceMethodInfo)
        {
            var assemblyName = traceMethodInfo.InvocationTargetType.Assembly.GetName().Name;
            if (_assemblies.TryGetValue(assemblyName, out var assemblyInfoCache))
            {
                if (assemblyInfoCache.MethodWrappers == null)
                {
                    lock (this)
                    {
                        assemblyInfoCache.MethodWrappers = GetMethodWrappers(assemblyInfoCache.Assembly);
                    }
                }

                foreach (var methodWrapper in assemblyInfoCache.MethodWrappers)
                {
                    if (methodWrapper.CanWrap(traceMethodInfo))
                    {
                        functionInfo.Wrapper = methodWrapper;
                        break;
                    }
                }
            }
            else
            {
                functionInfo.Wrapper = new NoopMethodWrapper();
            }
        }

        private static FunctionInfoCache GetFunctionInfoFromCache(uint functionToken, TraceMethodInfo traceMethodInfo)
        {
            var functionInfo = FunctionInfosCache.GetOrAdd(functionToken, (token) =>
            {
                var functionInfoCache = new FunctionInfoCache
                {
                    MethodBase = traceMethodInfo.InvocationTargetType.Module.ResolveMethod((int) token)
                };
                return functionInfoCache;
            });
            return functionInfo;
        }

        private List<IMethodWrapper> GetMethodWrappers(Assembly assembly)
        {
            List<IMethodWrapper> methodWrappers = new List<IMethodWrapper>();
            List<Type> wrapperTypes = GetWrapperTypes(assembly);
            foreach (var wrapperType in wrapperTypes)
            {
                var wrapper = (IMethodWrapper) Activator.CreateInstance(wrapperType, _tracer);
                methodWrappers.Add(wrapper);
            }

            return methodWrappers;
        }

        private static List<Type> GetWrapperTypes(Assembly assembly)
        {
            List<Type> wrapperTypes = new List<Type>();
            try
            {
                var types = assembly.GetTypes();
                foreach (var type in types)
                {
                    if (typeof(IMethodWrapper).IsAssignableFrom(type) && type.IsClass && !type.IsAbstract)
                    {
                        wrapperTypes.Add(type);
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Trace.WriteLine(ex);
            }
            return wrapperTypes;
        }

        private class FunctionInfoCache
        {
            public IMethodWrapper Wrapper { get; set; }

            public MethodBase MethodBase { get; set; }
        }

        private class AssemblyInfoCache
        {
            public Assembly Assembly { get; set; }

            public List<IMethodWrapper> MethodWrappers { get; set; }
        }
    }
}
