using System;
using System.Data.Common;
using System.Linq;
using ClrProfiler.Trace.Internal;
using OpenTracing;
using OpenTracing.Tag;

namespace ClrProfiler.Trace.Wrappers.Sql
{
    public class MySqlConnectorClient: IWrapper
    {
        private const string TypeName = "MySql.Data.MySqlClient.SqlCommand";
        private static readonly string[] AssemblyNames = { "MySqlConnector" };
        private static readonly string[] TraceMethods = { "ExecuteReaderAsync", "ExecuteNonQueryAsync", "ExecuteScalarAsync" };

        private const string TagMethod = "db.method";
    
        private readonly ITracer _tracer;

        public MySqlConnectorClient(ITracer tracer)
        {
            _tracer = tracer;
        }

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
            var dbCommand = (DbCommand)traceMethodInfo.InvocationTarget;
            var scope = _tracer.BuildSpan("mysql.command")
                .WithTag(Tags.SpanKind, Tags.SpanKindClient)
                .WithTag(Tags.Component, "MySqlConnector")
                .WithTag(Tags.DbInstance, dbCommand.Connection.ConnectionString)
                .WithTag(Tags.DbStatement, dbCommand.CommandText)
                .WithTag(TagMethod, traceMethodInfo.MethodName)
                .StartActive();

            traceMethodInfo.TraceContext = scope;

            return delegate (object returnValue, Exception ex)
            {
                DelegateHelper.AsyncTaskResultMethodEnd(Leave, traceMethodInfo, ex, returnValue);
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
