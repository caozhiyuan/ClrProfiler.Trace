using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Reflection;

namespace ClrProfiler.Trace
{
    public class MethodWrapperService
    {
        private class FunctionInfoCache
        {
            public IMethodWrapper Wrapper { get; set; }

            public MethodBase MethodBase { get; set; }
        }

        private static readonly ConcurrentDictionary<uint, Lazy<FunctionInfoCache>> FunctionInfosCache =
            new ConcurrentDictionary<uint, Lazy<FunctionInfoCache>>();

        private readonly IEnumerable<IMethodWrapper> _wrappers;

        public MethodWrapperService(IEnumerable<IMethodWrapper> wrappers)
        {
            _wrappers = wrappers;
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
            var lazyInfo = FunctionInfosCache.GetOrAdd(functionToken, (token) =>
            {
                return new Lazy<FunctionInfoCache>(() =>
                {
                    var functionInfoCache = new FunctionInfoCache();

                    var invocationTargetType = traceMethodInfo.InvocationTargetType;
                    functionInfoCache.MethodBase = invocationTargetType.Module.ResolveMethod((int)functionToken);

                    traceMethodInfo.MethodBase = functionInfoCache.MethodBase;

                    foreach (var wrapper in _wrappers)
                    {
                        if (wrapper.CanWrap(traceMethodInfo))
                        {
                            functionInfoCache.Wrapper = wrapper;
                            break;
                        }
                    }       
                    return functionInfoCache;
                });
            });

            traceMethodInfo.MethodBase = lazyInfo.Value.MethodBase;
            return lazyInfo.Value.Wrapper?.BeforeWrappedMethod(traceMethodInfo);
        }
    }
}
