using System;
using System.Reflection;
using Jaeger;
using Jaeger.Samplers;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.DependencyInjection.Extensions;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using OpenTracing;
using OpenTracing.Util;

namespace ClrProfiler.Trace.DependencyInjection
{
    public class ServiceLocator
    {
        private readonly IServiceProvider _serviceProvider;

        public static readonly ServiceLocator Instance = new ServiceLocator();

        private ServiceLocator()
        {
            var serviceCollection = new ServiceCollection();

            RegisterServices(serviceCollection);

            _serviceProvider = serviceCollection.BuildServiceProvider();
        }

        public T GetService<T>()
        {
            return (T)_serviceProvider.GetService(typeof(T));
        }

        private void RegisterServices(ServiceCollection services)
        {
            services.AddSingleton<MethodWrapperService>();
            var types = typeof(MethodWrapperService).Assembly.GetTypes();
            foreach (var type in types)
            {
                if (typeof(IMethodWrapper).IsAssignableFrom(type) && type.IsClass && !type.IsAbstract)
                {
                    services.Add(ServiceDescriptor.Singleton(typeof(IMethodWrapper), type));
                }
            }

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
    }
}
