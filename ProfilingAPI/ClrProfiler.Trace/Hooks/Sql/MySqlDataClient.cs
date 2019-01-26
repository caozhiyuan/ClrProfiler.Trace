using System;
using System.Linq;
using System.Threading.Tasks;

namespace ClrProfiler.Trace.Hooks.Sql
{
    public class MySqlDataClient : IWrapper
    {
        private const string TypeName = "MySql.Data.MySqlClient.MySqlCommand";
        private static readonly string[] AssemblyNames = { "MySql.Data" };
        private static readonly string[] TraceMethods = { "ExecuteReader", "ExecuteNonQuery", "ExecuteScalar" };

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
                Console.WriteLine($"returnValue:{returnValue}");
                Console.WriteLine($"ex:{ex}");
#endif
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
            if (AssemblyNames.Contains(assemblyName) && TypeName == traceMethodInfo.TypeName)
            {
                if (TraceMethods.Contains(traceMethodInfo.MethodName))
                {
                    return true;
                }
            }
            return false;
        }
    }
}
