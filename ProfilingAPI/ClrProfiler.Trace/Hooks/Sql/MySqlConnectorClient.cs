using System;
using System.Linq;

namespace ClrProfiler.Trace.Hooks.Sql
{
    public class MySqlConnectorClient: IWrapper
    {
        private const string TypeName = "MySql.Data.MySqlClient.SqlCommand";
        private static readonly string[] AssemblyNames = { "MySqlConnector" };
        private static readonly string[] TraceMethods = { "ExecuteReaderAsync", "ExecuteNonQueryAsync", "ExecuteScalarAsync" };

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
                TraceDelegateHelper.AsyncMethodEnd(Level, traceMethodInfo, ex, returnValue);
            };
        }

        private void Level(TraceMethodInfo traceMethodInfo, object ret, Exception ex)
        {
#if DEBUG
            Console.WriteLine($"returnValue:{ret}");
            Console.WriteLine($"ex:{ex}");
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
                if (TraceMethods.Contains(traceMethodInfo.MethodName))
                {
                    return true;
                }
            }
            return false;
        }
    }
}
