using System.Web;
using OpenTracing;
using OpenTracing.Propagation;
using OpenTracing.Tag;

namespace ClrProfiler.Trace.AspNet
{
    public class HttpApplicationExecuteStep : IMethodWrapper
    {
        private const string TypeName = "System.Web.HttpApplication";
        private const string AssemblyName = "System.Web";
        private const string MethodName = "ExecuteStep";

        private readonly ITracer _tracer;

        public HttpApplicationExecuteStep(ITracer tracer)
        {
            _tracer = tracer;
        }

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
            if (HttpRuntime.UsingIntegratedPipeline)
            {
                HttpContext context = (traceMethodInfo.InvocationTarget as HttpApplication)?.Context;
                if (context != null)
                {
                    if (context.Items["ClrProfiler.Trace.AspNet.TraceScope"] == null)
                    {
                        var extractedSpanContext = _tracer.Extract(BuiltinFormats.HttpHeaders, new RequestHeadersExtractAdapter(context.Request.Headers));

                        var scope = _tracer.BuildSpan("http.in")
                            .AsChildOf(extractedSpanContext)
                            .WithTag(Tags.SpanKind, Tags.SpanKindClient)
                            .WithTag(Tags.Component, "AspNet")
                            .WithTag(Tags.HttpMethod, context.Request.HttpMethod)
                            .WithTag(Tags.HttpUrl, context.Request.Path.ToString())
                            .WithTag(Tags.PeerHostname, context.Request.UserHostName)
                            .StartActive();

                        context.Items["ClrProfiler.Trace.AspNet.TraceScope"] = scope;
                    }
                }
            }
            return null;
        }

        public bool CanWrap(TraceMethodInfo traceMethodInfo)
        {
            var invocationTargetType = traceMethodInfo.Type;
            var assemblyName = invocationTargetType.Assembly.GetName().Name;
            if (assemblyName == AssemblyName && TypeName == invocationTargetType.FullName)
            {
                if (traceMethodInfo.MethodBase.Name == MethodName)
                {
                    return true;
                }
            }

            return false;
        }
    }
}
