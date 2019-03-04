using OpenTracing;

namespace ClrProfiler.Trace.HttpWebRequest
{
    public class HttpWebRequestSerializeHeaders: IMethodWrapper
    {
        private const string TypeName = "System.Net.HttpWebRequest";
        private const string AssemblyName = "System";
        private const string MethodName = "SerializeHeaders";

        private readonly ITracer _tracer;

        public HttpWebRequestSerializeHeaders(ITracer tracer)
        {
            _tracer = tracer;
        }

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
            var request = traceMethodInfo.InvocationTarget as System.Net.HttpWebRequest;
            if (request == null)
                return null;

            // how to add header?


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
