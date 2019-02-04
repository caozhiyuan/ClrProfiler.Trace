using System;
using System.IO;
using System.Reflection;
using ClrProfiler.Trace.DependencyInjection;
using Jaeger;
using Jaeger.Samplers;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using OpenTracing;
using OpenTracing.Util;

namespace ClrProfiler.Trace
{
    public delegate void EndMethodDelegate(object returnValue, Exception ex);

    public class TraceAgent
    {
        private static readonly TraceAgent Instance = new TraceAgent();

        private TraceAgent()
        {
            AppDomain.CurrentDomain.AssemblyResolve += CurrentDomain_AssemblyResolve;

            ServiceLocator.Instance.RegisterServices(RegisterServices);
        }

        private Assembly CurrentDomain_AssemblyResolve(object sender, ResolveEventArgs args)
        {
            var home = Environment.GetEnvironmentVariable("CLRPROFILER_HOME");
            if (!string.IsNullOrEmpty(home))
            {
                var filepath = Path.Combine(home, $"{new AssemblyName(args.Name).Name}.dll");
                if (File.Exists(filepath))
                {
                    return Assembly.LoadFrom(filepath);
                }
            }
            return null;
        }

        private void RegisterServices(ServiceCollection services)
        {
            services.AddSingleton<MethodFinderService>();

            services.AddSingleton<ILoggerFactory, NullLoggerFactory>();

            services.AddSingleton(serviceProvider =>
            {
                string serviceName = Assembly.GetEntryAssembly().GetName().Name;

                ILoggerFactory loggerFactory = serviceProvider.GetRequiredService<ILoggerFactory>();

                ISampler sampler = new ConstSampler(sample: true);

                ITracer tracer = new Tracer.Builder(serviceName)
                    .WithLoggerFactory(loggerFactory)
                    .WithSampler(sampler)
                    .Build();

                GlobalTracer.Register(tracer);

                return tracer;
            });
        }

        public static object GetInstance()
        {
            return Instance;
        }

        public object BeforeMethod(object type, object invocationTarget, object[] methodArguments, uint functionToken)
        {
            try
            {
                var args = methodArguments;
                var wrapperService = ServiceLocator.Instance.GetService<MethodFinderService>();
                var endMethodDelegate = wrapperService.BeforeWrappedMethod(type, invocationTarget, args, functionToken);
                return endMethodDelegate != null ? new MethodTrace(endMethodDelegate) : default(MethodTrace);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Trace.WriteLine(ex);
                return default(MethodTrace);
            }
        }
    }

    public class MethodTrace
    {
        private readonly EndMethodDelegate _endMethodDelegate;

        public MethodTrace(EndMethodDelegate endMethodDelegate)
        {
            this._endMethodDelegate = endMethodDelegate;
        }

        public void EndMethod(object returnValue, object ex)
        {
            this._endMethodDelegate(returnValue, (Exception)ex);
        }
    }
}
