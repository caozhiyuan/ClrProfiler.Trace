using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Text;

namespace ClrProfiler.Trace
{
    public class WrapperService
    {
        private static readonly ConcurrentDictionary<UInt32, IWrapper> WrappersCache =
            new ConcurrentDictionary<uint, IWrapper>();

        private static readonly Lazy<IWrapper[]> LazyWrappers = new Lazy<IWrapper[]>(ValueFactory);

        private static IWrapper[] ValueFactory()
        {
            List<IWrapper> wrappers = new List<IWrapper>();
            var types = typeof(WrapperService).Assembly.GetTypes();
            foreach (var type in types)
            {
                if (typeof(IWrapper).IsAssignableFrom(type) && type.IsClass && !type.IsAbstract)
                {
                    wrappers.Add((IWrapper)Activator.CreateInstance(type));
                }
            }
            return wrappers.ToArray();
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

            foreach (var wrapper in LazyWrappers.Value)
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
