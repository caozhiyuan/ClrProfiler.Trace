using System;
using System.Threading.Tasks;

namespace ClrProfiler.Trace.Hooks
{
    public delegate void AsyncMethodEndDelegate(TraceMethodInfo traceMethodInfo, object returnValue, Exception ex);

    public static class TraceDelegateHelper
    {
        private static readonly TaskCanceledException CanceledException = new TaskCanceledException();

        public static void AsyncTaskResultMethodEnd(AsyncMethodEndDelegate leave,
            TraceMethodInfo traceMethodInfo,
            Exception ex, 
            object returnValue)
        {
            AsyncMethodEnd(leave, traceMethodInfo, ex, returnValue);
        }

        public static void AsyncTaskMethodEnd(AsyncMethodEndDelegate leave,
            TraceMethodInfo traceMethodInfo,
            Exception ex, 
            object returnValue)
        {
            AsyncMethodEnd(leave, traceMethodInfo, ex, returnValue, false);
        }

        private static void AsyncMethodEnd(AsyncMethodEndDelegate endMethodDelegate, 
            TraceMethodInfo traceMethodInfo, 
            Exception ex,
            object returnValue,
            bool hasResult = true)
        {
            if (ex != null)
            {
                endMethodDelegate(traceMethodInfo, null, ex);
                return;
            }

            if (returnValue != null)
            {
                var tcs = new TaskCompletionSource<dynamic>();
                ((Task)returnValue).ContinueWith(n =>
                {
                    if (n.IsFaulted)
                    {
                        var ex0 = n.Exception?.InnerException ?? new Exception("unknown exception");
                        endMethodDelegate(traceMethodInfo, null, ex0);
                        tcs.SetException(ex0);
                    }
                    else
                    {
                        if (n.IsCanceled)
                        {
                            endMethodDelegate(traceMethodInfo, null, CanceledException);
                            tcs.SetCanceled();
                        }
                        else
                        {
                            if (hasResult)
                            {
                                var ret = ((dynamic)n).Result;
                                endMethodDelegate(traceMethodInfo, ret, null);
                                tcs.SetResult(ret);
                            }
                            else
                            {
                                endMethodDelegate(traceMethodInfo, null, null);
                                tcs.SetResult(1);
                            }
                        }
                    }
                }, TaskScheduler.Default);
                returnValue = tcs.Task;
            }
        }
    }
}
