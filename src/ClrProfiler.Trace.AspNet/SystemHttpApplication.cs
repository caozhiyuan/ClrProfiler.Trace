using System;
using OpenTracing;

namespace ClrProfiler.Trace.AspNet
{
    public class SystemHttpApplication
    {
        private const string TypeName = "System.Web.HttpApplication";
        private const string AssemblyName = "System.Web";
        private const string MethodName = "InitModulesCommon";

        private readonly ITracer _tracer;

        public SystemHttpApplication(ITracer tracer)
        {
            _tracer = tracer;
        }

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {

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
