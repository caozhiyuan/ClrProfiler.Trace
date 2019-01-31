using System;
using ClrProfiler.Trace.DependencyInjection;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.DependencyInjection;

namespace ClrProfiler.Trace.MethodWrappers.AspNetCore
{
    public class WebHostBuilder : IMethodWrapper
    {
        private const string TypeName = "Microsoft.AspNetCore.Hosting.WebHostBuilder";
        private const string AssemblyName = "Microsoft.AspNetCore.Hosting";
        private const string MethodName = "BuildCommonServices";

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
            var invocationTargetType = traceMethodInfo.InvocationTargetType;
            var assemblyName = invocationTargetType.Assembly.GetName().Name;
            if (assemblyName == AssemblyName && TypeName == invocationTargetType.FullName)
            {
                if (traceMethodInfo.MethodBase.Name == MethodName)
                {
                    return true;
                }
            }

            return false;
        }
    }
}
