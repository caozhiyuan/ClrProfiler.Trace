using System;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Builder;
using ClrProfiler.Trace.DependencyInjection;

namespace ClrProfiler.Trace.Wrappers.AspNetCore
{
    public class ProfilerStartupFilter: IStartupFilter
    {
        private readonly ServiceLocator _serviceLocator;

        public ProfilerStartupFilter(ServiceLocator serviceLocator)
        {
            _serviceLocator = serviceLocator;
        }

        public Action<IApplicationBuilder> Configure(Action<IApplicationBuilder> next)
        {
            return applicationBuilder =>
            {
                applicationBuilder.UseMiddleware<ProfilerMiddleWare>(_serviceLocator);
                next(applicationBuilder);
            };
        }
    }
}
