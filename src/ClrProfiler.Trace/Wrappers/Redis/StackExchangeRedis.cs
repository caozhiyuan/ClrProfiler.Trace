using System;
using System.Linq;
using ClrProfiler.Trace.Internal;
using OpenTracing;
using OpenTracing.Tag;

namespace ClrProfiler.Trace.Wrappers.Redis
{
    public class StackExchangeRedis : IWrapper
    {
        private const string TypeName = "StackExchange.Redis.ConnectionMultiplexer";
        private static readonly string[] AssemblyNames = new[] { "StackExchange.Redis", "StackExchange.Redis.StrongName" };

        private const string ExecuteAsyncImpl = "ExecuteAsyncImpl";
        private const string ExecuteSyncImpl = "ExecuteSyncImpl";
        private static readonly PropertyFetcher ConfigPropertyFetcher = new PropertyFetcher("Configuration");
        private static readonly PropertyFetcher CommandAndKeyPropertyFetcher = new PropertyFetcher("CommandAndKey");
        
        private readonly ITracer _tracer;

        public StackExchangeRedis(ITracer tracer)
        {
            _tracer = tracer;
        }

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
            var multiplexer = traceMethodInfo.InvocationTarget;
            var message = traceMethodInfo.MethodArguments[0];

            var config = (string)ConfigPropertyFetcher.Fetch(multiplexer);
            var hostAndPort = GetHostAndPort(config);
            var rawCommand = (string)CommandAndKeyPropertyFetcher.Fetch(message);

            var scope = _tracer.BuildSpan("redis.command")
                .WithTag(Tags.SpanKind, Tags.SpanKindClient)
                .WithTag(Tags.Component, "StackExchange.Redis")
                .WithTag("redis.raw_command", rawCommand)
                .WithTag("out.host", hostAndPort.Item1)
                .WithTag("out.port", hostAndPort.Item2)
                .StartActive();

            traceMethodInfo.TraceContext = scope;

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
                    DelegateHelper.AsyncTaskResultMethodEnd(Leave, traceMethodInfo, ex, returnValue);
                };
            }
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


        /// <summary>
        /// Get the host and port from the config
        /// </summary>
        /// <param name="config">The config</param>
        /// <returns>The host and port</returns>
        private static Tuple<string, string> GetHostAndPort(string config)
        {
            string host = null;
            string port = null;

            if (config != null)
            {
                // config can contain several settings separated by commas:
                // hostname:port,name=MyName,keepAlive=180,syncTimeout=10000,abortConnect=False
                // split in commas, find the one without '=', split that one on ':'
                string[] hostAndPort = config.Split(',')
                    .FirstOrDefault(p => !p.Contains("="))
                    ?.Split(':');

                if (hostAndPort != null)
                {
                    host = hostAndPort[0];
                }

                // check length because port is optional
                if (hostAndPort?.Length > 1)
                {
                    port = hostAndPort[1];
                }
            }

            return new Tuple<string, string>(host, port);
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
