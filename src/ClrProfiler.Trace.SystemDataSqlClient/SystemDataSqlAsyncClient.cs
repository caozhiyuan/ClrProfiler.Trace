using System;
using System.Data.Common;
using System.Linq;
using ClrProfiler.Trace.Extensions;
using ClrProfiler.Trace.Utils;
using OpenTracing;
using OpenTracing.Tag;

namespace ClrProfiler.Trace.SystemDataSqlClient
{
    public class SystemDataSqlAsyncClient : IMethodWrapper
    {
        private const string TypeName = "System.Data.SqlClient.SqlCommand";
        private static readonly string[] AssemblyNames = {"System.Data", "System.Data.SqlClient"};
        private static readonly string[] TraceMethods = { "ExecuteReaderAsync", "ExecuteNonQueryAsync", "ExecuteScalarAsync", "ExecuteXmlReaderAsync" };

        private const string TagMethod = "db.method";
        private const string TagIsAsync = "db.async";

        private readonly ITracer _tracer;

        public SystemDataSqlAsyncClient(ITracer tracer)
        {
            _tracer = tracer;
        }

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
            var dbCommand = (DbCommand)traceMethodInfo.InvocationTarget;
            var scope = _tracer.BuildSpan("mssql.command")
                .WithTag(Tags.SpanKind, Tags.SpanKindClient)
                .WithTag(Tags.Component, "SqlServer")
                .WithTag(Tags.DbInstance, dbCommand.Connection.ConnectionString)
                .WithTag(Tags.DbStatement, dbCommand.CommandText)
                .WithTag(TagMethod, traceMethodInfo.MethodBase.Name)
                .WithTag(TagIsAsync, true)
                .StartActive(false);

            traceMethodInfo.TraceContext = scope;

            return delegate (object returnValue, Exception ex)
            {
                DelegateHelper.AsyncMethodEnd(Leave, traceMethodInfo, ex, returnValue);

                // for async method , at method end restore active scope, important
                var tempScope = (IScope)traceMethodInfo.TraceContext;
                tempScope.Dispose();
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
            var invocationTargetType = traceMethodInfo.Type;
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
