using System;
using System.Collections.Concurrent;
using System.Collections.Generic;

namespace ClrProfiler.Trace
{
    public class WrapperService
    {
        private static readonly ConcurrentDictionary<UInt32, IWrapper> WrappersCache =
            new ConcurrentDictionary<uint, IWrapper>();

        private readonly IEnumerable<IWrapper> _wrappers;

        public WrapperService(IEnumerable<IWrapper> wrappers)
        {
            _wrappers = wrappers;
        }

        public EndMethodDelegate BeforeWrappedMethod(string typeName, string methodName,
            object invocationTarget,
            object[] methodArguments,
            UInt32 functionToken)
        {
            var traceMethodInfo = new TraceMethodInfo
            {
                TypeName = typeName,
                MethodName = methodName,
                InvocationTarget = invocationTarget,
                FunctionToken = functionToken,
                MethodArguments = methodArguments
            };

            if (WrappersCache.TryGetValue(functionToken, out var foundWrapper))
            {
                return foundWrapper.BeforeWrappedMethod(traceMethodInfo);
            }

            foreach (var wrapper in _wrappers)
            {
                if (wrapper.CanWrap(traceMethodInfo))
                {
                    WrappersCache.TryAdd(functionToken, wrapper);
                    break;
                }
            }

            if (WrappersCache.TryGetValue(functionToken, out foundWrapper))
            {
                return foundWrapper.BeforeWrappedMethod(traceMethodInfo);
            }
            return null;
        }
    }

}
