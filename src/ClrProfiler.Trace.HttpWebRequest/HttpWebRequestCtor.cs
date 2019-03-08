using OpenTracing;

namespace ClrProfiler.Trace.HttpWebRequest
{
    public class HttpWebRequestCtor: IMethodWrapper
    {
        private const string TypeName = "System.Net.HttpWebRequest";
        private const string AssemblyName = "System";
        private const string MethodName = ".ctor";

        private readonly ITracer _tracer;

        public HttpWebRequestCtor(ITracer tracer)
        {
            _tracer = tracer;
        }

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
            HttpWebRequestDiagnostic.Instance.Initialize(_tracer);
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
