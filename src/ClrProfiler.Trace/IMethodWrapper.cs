namespace ClrProfiler.Trace
{
    public interface IMethodWrapper
    {
        EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo);

        bool CanWrap(TraceMethodInfo traceMethodInfo);
    }
}
