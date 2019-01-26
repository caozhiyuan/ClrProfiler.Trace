using System;
using System.Threading.Tasks;

namespace ClrProfiler.Trace.Hooks.Http
{
    public class SystemHttpClient: IWrapper
    {
        private const string TypeName = "System.Net.Http.HttpClient";
        private const string AssemblyName = "System.Net.Http";

        private const string SendAsync = "SendAsync";

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
#if DEBUG
            Console.WriteLine($"typeName;{traceMethodInfo.TypeName} methodName:{traceMethodInfo.MethodName}");
            if (traceMethodInfo.MethodArguments != null)
            {
                Console.WriteLine("methodArguments:");
                foreach (var methodArgument in traceMethodInfo.MethodArguments)
                {
                    Console.WriteLine(methodArgument);
                }
            }
#endif
            return delegate (object returnValue, Exception ex)
            {
#if DEBUG
                Console.WriteLine($"ex:{ex}");
#endif
                ((Task)returnValue).ContinueWith(n =>
                {
                    if (n.IsFaulted)
                    {
                        Console.WriteLine($"ex:{ex}");
                    }
                    var ret = ((dynamic)n).Result;
                    Console.WriteLine($"returnValue:{ret}");
                }, TaskContinuationOptions.ExecuteSynchronously);
            };
        }

        public bool CanWrap(TraceMethodInfo traceMethodInfo)
        {
            if (traceMethodInfo.InvocationTarget == null)
            {
                return false;
            }
            var invocationTargetType = traceMethodInfo.InvocationTarget.GetType();
            var assemblyName = invocationTargetType.Assembly.GetName().Name;
            if (assemblyName == AssemblyName && TypeName == traceMethodInfo.TypeName)
            {
                if (traceMethodInfo.MethodName == SendAsync)
                {
                    return true;
                }
            }

            return false;
        }
    }
}
