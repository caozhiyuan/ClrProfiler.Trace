namespace ClrProfiler.Trace
{
    public interface IWrapper
    {
        EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo);

        bool CanWrap(TraceMethodInfo traceMethodInfo);
    }
}
