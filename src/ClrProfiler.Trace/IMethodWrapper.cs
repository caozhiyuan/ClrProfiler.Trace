using System;

namespace ClrProfiler.Trace
{
    public interface IMethodWrapper
    {
        EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo);

        bool CanWrap(TraceMethodInfo traceMethodInfo);
    }

    public class NoopMethodWrapper : IMethodWrapper
    {
        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
            return null;
        }

        public bool CanWrap(TraceMethodInfo traceMethodInfo)
        {
            return true;
        }
    }
}
