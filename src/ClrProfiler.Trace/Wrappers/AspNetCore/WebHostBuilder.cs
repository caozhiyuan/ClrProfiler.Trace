using System;
using ClrProfiler.Trace.DependencyInjection;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.DependencyInjection;

namespace ClrProfiler.Trace.Wrappers.AspNetCore
{
    public class WebHostBuilder : IWrapper
    {
        private const string TypeName = "Microsoft.AspNetCore.Hosting.WebHostBuilder";
        private const string AssemblyName = "Microsoft.AspNetCore.Hosting";
        private const string SendAsync = "BuildCommonServices";

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
            return delegate (object returnValue, Exception ex)
            {
                Leave(traceMethodInfo, returnValue, ex);
            };
        }

        private void Leave(TraceMethodInfo traceMethodInfo, object ret, Exception ex)
        {
            var serviceCollection = (ServiceCollection) ret;
            serviceCollection.AddSingleton<IStartupFilter>(n => new ProfilerStartupFilter(ServiceLocator.Instance));
        }

        public bool CanWrap(TraceMethodInfo traceMethodInfo)
        {
            if (traceMethodInfo.InvocationTarget == null)
            {
                return false;
            }
            var invocationTargetType = traceMethodInfo.InvocationTarget.GetType();
            var assemblyName = invocationTargetType.Assembly.GetName().Name;
            if (assemblyName == AssemblyName && TypeName == traceMethodInfo.TypeName)
            {
                if (traceMethodInfo.MethodName == SendAsync)
                {
                    return true;
                }
            }

            return false;
        }
    }
}
