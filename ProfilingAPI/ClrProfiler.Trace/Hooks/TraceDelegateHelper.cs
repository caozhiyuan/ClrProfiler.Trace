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
                returnValue = ((Task)returnValue).ContinueWith(n =>
                {
                    if (n.IsFaulted)
                    {
                        endMethodDelegate(traceMethodInfo,null, ex);
                        return Task.FromException(n.Exception ?? new Exception("unknown exception"));
                    }

                    var ret = ((dynamic)n).Result;
                    endMethodDelegate(traceMethodInfo, ret, null);
                    return ret;
                }, TaskContinuationOptions.ExecuteSynchronously);
            }
        }
    }
}
