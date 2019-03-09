using System;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;

namespace ClrProfiler.Trace.Utils
{
    public delegate void AsyncMethodEndDelegate(TraceMethodInfo traceMethodInfo, object returnValue, Exception ex);

    public static class DelegateHelper
    {
        private static readonly TaskCanceledException CanceledException = new TaskCanceledException();

        public static void AsyncMethodEnd(AsyncMethodEndDelegate endMethodDelegate, 
            TraceMethodInfo traceMethodInfo, 
            Exception ex,
            object returnValue)
        {
            if (ex != null)
            {
                endMethodDelegate(traceMethodInfo, null, ex);
                return;
            }

            if (returnValue != null)
            {
                if (SynchronizationContext.Current != null)
                {
                    ((Task)returnValue).ContinueWith(n =>
                    {
                        TraceEnd(endMethodDelegate, traceMethodInfo, n);
                    }, TaskScheduler.FromCurrentSynchronizationContext());
                }
                else
                {
                    ((Task)returnValue).ContinueWith(n =>
                    {
                        TraceEnd(endMethodDelegate, traceMethodInfo, n);
                    }, TaskContinuationOptions.ExecuteSynchronously);
                }
            }
        }

        private static void TraceEnd(AsyncMethodEndDelegate endMethodDelegate, 
            TraceMethodInfo traceMethodInfo,
            Task n)
        {
            if (n.IsFaulted)
            {
                var ex0 = n.Exception?.InnerException ?? new Exception("unknown exception");
                endMethodDelegate(traceMethodInfo, null, ex0);
            }
            else
            {
                if (n.IsCanceled)
                {
                    endMethodDelegate(traceMethodInfo, null, CanceledException);
                }
                else
                {
                    var methodInfo = traceMethodInfo.MethodBase as MethodInfo;
                    if (methodInfo != null && methodInfo.ReturnType == typeof(Task))
                    {
                        endMethodDelegate(traceMethodInfo, null, null);
                    }
                    else
                    {
                        var ret = ((dynamic) n).Result;
                        endMethodDelegate(traceMethodInfo, ret, null);
                    }
                }
            }
        }
    }
}
