using System;

namespace Datadog.Trace.ClrProfiler
{
    public delegate void EndMethodDelegate(object returnValue, Exception ex);

    public class TraceAgent
    {
        private static readonly TraceAgent Instance = new TraceAgent();
        private readonly WrapperService _wrapperService;

        private TraceAgent()
        {
            _wrapperService = new WrapperService();
        }

        public static object GetInstance()
        {
            return Instance;
        }

        public object BeforeMethod(string methodName, object invocationTarget, object[] methodArguments)
        {
            try
            {
                var args = methodArguments;
                var endMethodDelegate = this._wrapperService.BeforeWrappedMethod(methodName, invocationTarget, args);
                return ((endMethodDelegate != null) ? new MethodTrace(endMethodDelegate) : null);
            }
            catch (Exception ex)
            {
                return null;
            }
        }
    }

    public class WrapperService
    {
        public EndMethodDelegate BeforeWrappedMethod(string methodName, object invocationTarget, object[] methodArguments)
        {
            Console.WriteLine("in");
            if (methodArguments != null)
            {
                foreach (var methodArgument in methodArguments)
                {
                    Console.WriteLine(methodArgument);
                }
            }

            return delegate(object returnValue, Exception ex)
            {
                Console.WriteLine("out");
                Console.WriteLine(returnValue);
            };
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
