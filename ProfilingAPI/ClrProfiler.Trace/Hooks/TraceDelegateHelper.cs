using System;
using System.Threading.Tasks;

namespace ClrProfiler.Trace.Hooks
{
    public delegate void AsyncMethodEndDelegate(TraceMethodInfo traceMethodInfo, object returnValue, Exception ex);

    public static class TraceDelegateHelper
    {
        public static void AsyncMethodEnd(AsyncMethodEndDelegate endMethodDelegate, TraceMethodInfo traceMethodInfo , Exception ex, object returnValue)
        {
            endMethodDelegate(traceMethodInfo,null, ex);

            if (returnValue != null)
            {
                var value = returnValue;
                returnValue = ((Task)value).ContinueWith(n =>
                {
                    if (n.IsFaulted)
                    {
                        endMethodDelegate(traceMethodInfo,null, ex);
                    }

                    var ret = ((dynamic)n).Result;
                    endMethodDelegate(traceMethodInfo, ret, null);
                }, TaskContinuationOptions.ExecuteSynchronously);
            }
        }
    }
}
