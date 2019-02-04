using System;
using System.Reflection;

namespace ClrProfiler.Trace
{
    public class TraceMethodInfo
    {
        public Type Type { get; set; }

        public object InvocationTarget { get; set; }

        public object[] MethodArguments { get; set; }

        public MethodBase MethodBase { get; set; }

        public object TraceContext { get; set; }
    }
}
