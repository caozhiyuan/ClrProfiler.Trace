using System;
using System.Linq;

namespace ClrProfiler.Trace.Hooks.Redis
{
    public class StackExchangeRedis : IWrapper
    {
        private const string TypeName = "StackExchange.Redis.ConnectionMultiplexer";
        private static readonly string[] AssemblyNames = new[] { "StackExchange.Redis", "StackExchange.Redis.StrongName" };

        private const string ExecuteAsyncImpl = "ExecuteAsyncImpl";
        private const string ExecuteSyncImpl = "ExecuteSyncImpl";

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
#if DEBUG
            Console.WriteLine($"typeName;{traceMethodInfo.TypeName} methodName:{traceMethodInfo.MethodName}");
            if (traceMethodInfo.MethodArguments!= null)
            {
                Console.WriteLine("methodArguments:");
                foreach (var methodArgument in traceMethodInfo.MethodArguments)
                {
                    Console.WriteLine(methodArgument);
                }
            }
#endif
            if (traceMethodInfo.MethodName == ExecuteSyncImpl)
            {
                return delegate (object returnValue, Exception ex)
                {
                    Leave(traceMethodInfo, returnValue, ex);
                };
            }
            else
            {
                return delegate (object returnValue, Exception ex)
                {
                    TraceDelegateHelper.AsyncTaskResultMethodEnd(Leave, traceMethodInfo, ex, returnValue);
                };
            }
        }

        private void Leave(TraceMethodInfo traceMethodInfo, object ret, Exception ex)
        {
#if DEBUG
            Console.WriteLine($"returnValue:{ret},ex:{ex}");
#endif
        }

        public bool CanWrap(TraceMethodInfo traceMethodInfo)
        {
            if (traceMethodInfo.InvocationTarget == null)
            {
                return false;
            }
            var invocationTargetType = traceMethodInfo.InvocationTarget.GetType();
            var assemblyName = invocationTargetType.Assembly.GetName().Name;
            if (AssemblyNames.Contains(assemblyName) && TypeName == traceMethodInfo.TypeName)
            {
                if (traceMethodInfo.MethodName == ExecuteAsyncImpl || traceMethodInfo.MethodName == ExecuteSyncImpl)
                {
                    return true;
                }
            }
            return false;
        }
    }
}
