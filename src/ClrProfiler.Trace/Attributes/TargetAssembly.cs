using System;

namespace ClrProfiler.Trace.Attributes
{
    [AttributeUsage(AttributeTargets.Assembly)]
    public class TargetAssemblyAttribute: Attribute
    {
        public string[] Names { get; set; }
    }
}
