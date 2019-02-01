using System;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using OpenTracing;

namespace ClrProfiler.Trace.AspNetCore
{
    public class ProfilerStartupFilter: IStartupFilter
    {
        private readonly ITracer _tracer;

        public ProfilerStartupFilter(ITracer tracer)
        {
            _tracer = tracer;
        }

        public Action<IApplicationBuilder> Configure(Action<IApplicationBuilder> next)
        {
            return applicationBuilder =>
            {
                applicationBuilder.UseMiddleware<ProfilerMiddleWare>(_tracer);
                next(applicationBuilder);
            };
        }
    }
}
