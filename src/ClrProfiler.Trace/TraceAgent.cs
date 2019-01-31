using System;
using System.IO;
using System.Reflection;
using ClrProfiler.Trace.DependencyInjection;

namespace ClrProfiler.Trace
{
    public delegate void EndMethodDelegate(object returnValue, Exception ex);

    public class TraceAgent
    {
        private static readonly TraceAgent Instance = new TraceAgent();

        private TraceAgent()
        {
            AssemblyResolver.Instance.Init();
        }

        public static object GetInstance()
        {
            return Instance;
        }

        public object BeforeMethod(object invocationTarget, object[] methodArguments, uint functionToken)
        {
            try
            {
                var args = methodArguments;
                var wrapperService = ServiceLocator.Instance.GetService<MethodWrapperService>();
                var endMethodDelegate = wrapperService.BeforeWrappedMethod(invocationTarget, args, functionToken);
                return endMethodDelegate != null ? new MethodTrace(endMethodDelegate) : default(MethodTrace);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Trace.WriteLine(ex);
                return default(MethodTrace);
            }
        }
    }

    public class MethodTrace
    {
        private readonly EndMethodDelegate _endMethodDelegate;

        public MethodTrace(EndMethodDelegate endMethodDelegate)
        {
            this._endMethodDelegate = endMethodDelegate;
        }

        public void EndMethod(object returnValue, object ex)
        {
            this._endMethodDelegate(returnValue, (Exception)ex);
        }
    }
}
