using System;
using System.Data.Common;
using System.Linq;
using ClrProfiler.Trace.Extensions;
using OpenTracing;
using OpenTracing.Tag;

namespace ClrProfiler.Trace.MethodWrappers.Sql
{
    public class MySqlDataClient : IMethodWrapper
    {
        private const string TypeName = "MySql.Data.MySqlClient.MySqlCommand";
        private static readonly string[] AssemblyNames = { "MySql.Data" };
        private static readonly string[] TraceMethods = { "ExecuteReader", "ExecuteNonQuery", "ExecuteScalar" };

        private const string TagMethod = "db.method";

        private readonly ITracer _tracer;

        public MySqlDataClient(ITracer tracer)
        {
            _tracer = tracer;
        }

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
            var dbCommand = (DbCommand)traceMethodInfo.InvocationTarget;
            var scope = _tracer.BuildSpan("mysql.command")
                .WithTag(Tags.SpanKind, Tags.SpanKindClient)
                .WithTag(Tags.Component, "MySql.Data")
                .WithTag(Tags.DbInstance, dbCommand.Connection.ConnectionString)
                .WithTag(Tags.DbStatement, dbCommand.CommandText)
                .WithTag(TagMethod, traceMethodInfo.MethodBase.Name)
                .StartActive();

            traceMethodInfo.TraceContext = scope;

            return delegate (object returnValue, Exception ex)
            {
                Leave(traceMethodInfo,returnValue,ex);
            };
        }

        private void Leave(TraceMethodInfo traceMethodInfo, object ret, Exception ex)
        {
            var scope = (IScope)traceMethodInfo.TraceContext;
            if (ex != null)
            {
                scope.Span.SetException(ex);
            }
            scope.Span.Finish();
        }

        public bool CanWrap(TraceMethodInfo traceMethodInfo)
        {
            var invocationTargetType = traceMethodInfo.InvocationTargetType;
            var assemblyName = invocationTargetType.Assembly.GetName().Name;
            if (AssemblyNames.Contains(assemblyName) && TypeName == invocationTargetType.FullName)
            {
                if (TraceMethods.Contains(traceMethodInfo.MethodBase.Name))
                {
                    return true;
                }
            }
            return false;
        }
    }
}
