using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using OpenTracing;

namespace ClrProfiler.Trace
{
    public class MethodFinderService
    {
        private static readonly ConcurrentDictionary<uint, FunctionInfoCache> FunctionInfosCache =
            new ConcurrentDictionary<uint, FunctionInfoCache>();

        private readonly ConcurrentDictionary<string, AssemblyInfoCache> _assemblies = 
            new ConcurrentDictionary<string, AssemblyInfoCache>();

        private readonly string _home;
        private readonly ITracer _tracer;

        public MethodFinderService(ITracer tracer)
        {
            _tracer = tracer;
            _home = Environment.GetEnvironmentVariable("CLRPROFILER_HOME");
            if (string.IsNullOrEmpty(_home))
            {
                throw new ArgumentException("CLR PROFILER HOME IsNullOrEmpty");
            }

            var path = Path.Combine(_home, "trace.json");
            if (File.Exists(path))
            {
                var text = File.ReadAllText(path);
                var jObject = (JObject)JsonConvert.DeserializeObject(text);
                foreach (var jToken in jObject["instrumentation"])
                {
                    _assemblies.TryAdd(jToken["assemblyName"].ToString(), new AssemblyInfoCache
                    {
                        AssemblyName = jToken["targetAssemblyName"].ToString()
                    });
                }
            }
        }

        public EndMethodDelegate BeforeWrappedMethod(object type,
            object invocationTarget,
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
                MethodArguments = methodArguments,
                Type = (Type) type
            };

            var functionInfo = GetFunctionInfoFromCache(functionToken, traceMethodInfo);
            traceMethodInfo.MethodBase = functionInfo.MethodBase;

            if (functionInfo.MethodWrapper == null)
            {
                PrepareMethodWrapper(functionInfo, traceMethodInfo);
            }
            
            return functionInfo.MethodWrapper?.BeforeWrappedMethod(traceMethodInfo);
        }

        /// <summary>
        /// Prepare FunctionInfoCache MethodWrapperInfo
        /// </summary>
        /// <param name="functionInfo"></param>
        /// <param name="traceMethodInfo"></param>
        private void PrepareMethodWrapper(FunctionInfoCache functionInfo, TraceMethodInfo traceMethodInfo)
        {
            try
            {
                var assemblyName = traceMethodInfo.Type.Assembly.GetName().Name;
                if (_assemblies.TryGetValue(assemblyName, out var assemblyInfoCache))
                {
                    if (assemblyInfoCache.Assembly == null)
                    {
                        lock (assemblyInfoCache)
                        {
                            if (assemblyInfoCache.Assembly == null)
                            {
                                var path = Path.Combine(_home, $"{assemblyInfoCache.AssemblyName}.dll");
                                if (File.Exists(path))
                                {
                                    var assembly = Assembly.LoadFile(path);
#if !NETSTANDARD
                                    AppDomain.CurrentDomain.Load(assembly.GetName());
#endif
                                    assemblyInfoCache.Assembly = assembly;
                                }
                                else
                                {
                                    throw new FileNotFoundException($"FileNotFound Path:{path}");
                                }
                            }
                        }
                    }

                    if (assemblyInfoCache.MethodWrappers == null)
                    {
                        lock (assemblyInfoCache)
                        {
                            if (assemblyInfoCache.MethodWrappers == null)
                            {
                                assemblyInfoCache.MethodWrappers = GetMethodWrappers(assemblyInfoCache.Assembly);
                            }
                        }
                    }

                    foreach (var methodWrapper in assemblyInfoCache.MethodWrappers)
                    {
                        if (methodWrapper.CanWrap(traceMethodInfo))
                        {
                            functionInfo.MethodWrapper = methodWrapper;
                            break;
                        }
                    }
                }
                if (functionInfo.MethodWrapper == null)
                {
                    functionInfo.MethodWrapper = new NoopMethodWrapper();
                }
            }
            catch (BadImageFormatException)
            {
                functionInfo.MethodWrapper = new NoopMethodWrapper();
            }
            catch (FileNotFoundException)
            {
                functionInfo.MethodWrapper = new NoopMethodWrapper();
            }
        }

        /// <summary>
        /// GetFunctionInfo MethodBase FromCache
        /// </summary>
        /// <param name="functionToken"></param>
        /// <param name="traceMethodInfo"></param>
        /// <returns></returns>
        private FunctionInfoCache GetFunctionInfoFromCache(uint functionToken, TraceMethodInfo traceMethodInfo)
        {
            var functionInfo = FunctionInfosCache.GetOrAdd(functionToken, token =>
            {
                var methodBase = traceMethodInfo.Type.Module.ResolveMethod((int) token);
                var functionInfoCache = new FunctionInfoCache
                {
                    MethodBase = methodBase
                };
                return functionInfoCache;
            });
            return functionInfo;
        }

        /// <summary>
        /// GetMethodWrappers
        /// </summary>
        /// <param name="assembly"></param>
        /// <returns></returns>
        private List<IMethodWrapper> GetMethodWrappers(Assembly assembly)
        {
            var methodWrappers = new List<IMethodWrapper>();
            var methodWrapperTypes = GetMethodWrapperTypes(assembly);
            foreach (var methodWrapperType in methodWrapperTypes)
            {
                var wrapper = (IMethodWrapper) Activator.CreateInstance(methodWrapperType, _tracer);
                methodWrappers.Add(wrapper);
            }
            return methodWrappers;
        }

        /// <summary>
        /// GetMethodWrapperTypes
        /// </summary>
        /// <param name="assembly"></param>
        /// <returns></returns>
        private List<Type> GetMethodWrapperTypes(Assembly assembly)
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
            public IMethodWrapper MethodWrapper { get; set; }

            public MethodBase MethodBase { get; set; }
        }

        private class AssemblyInfoCache
        {
            public Assembly Assembly { get; set; }

            public string AssemblyName { get; set; }

            public List<IMethodWrapper> MethodWrappers { get; set; }
        }
    }
}
