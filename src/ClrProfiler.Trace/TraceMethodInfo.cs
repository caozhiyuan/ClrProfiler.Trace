using System;
using System.Reflection;

namespace ClrProfiler.Trace
{
    public class TraceMethodInfo
    {
        private Type _invocationTargetType;

        public Type InvocationTargetType
        {
            get
            {
                if (_invocationTargetType != null)
                    return _invocationTargetType;

                _invocationTargetType = InvocationTarget.GetType();
                return _invocationTargetType;
            }
        }

        public object InvocationTarget { get; set; }

        public object[] MethodArguments { get; set; }

        public MethodBase MethodBase { get; set; }

        public object TraceContext { get; set; }
    }
}
