namespace ClrProfiler.Trace.Constants
{
    public class TraceConstant
    {
#if NET
        public const string PROFILER_HOME = "COR_PROFILER_HOME";
#else
        public const string PROFILER_HOME = "CORECLR_PROFILER_HOME";
#endif
    }
}
