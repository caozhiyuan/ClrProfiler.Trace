using System.Web;
using OpenTracing;

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
                var scope = context?.Items["ClrProfiler.Trace.AspNet.TraceScope"] as IScope;
                scope?.Span.Finish();
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
