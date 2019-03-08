using System.Web;
using OpenTracing;
using OpenTracing.Tag;

namespace ClrProfiler.Trace.AspNet
{
    public class HttpContextFinishPipelineRequest : IMethodWrapper
    {
        private const string TypeName = "System.Web.HttpContext";
        private const string AssemblyName = "System.Web";
        private const string MethodName = "FinishPipelineRequest";

        private readonly ITracer _tracer;

        public HttpContextFinishPipelineRequest(ITracer tracer)
        {
            _tracer = tracer;
        }

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
            if (HttpRuntime.UsingIntegratedPipeline)
            {
                HttpContext context = traceMethodInfo.InvocationTarget as HttpContext;
                if (context != null)
                {
                    var scope = context.Items["ClrProfiler.Trace.AspNet.TraceScope"] as IScope;
                    context.Items.Remove("ClrProfiler.Trace.AspNet.TraceScope");
                    if (scope != null)
                    {
                        scope.Span.SetTag(Tags.HttpStatus, context.Response.StatusCode);
                        if (context.Response.StatusCode >= 400)
                        {
                            scope.Span.SetTag(Tags.Error, true);
                        }
                        scope.Dispose();
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
