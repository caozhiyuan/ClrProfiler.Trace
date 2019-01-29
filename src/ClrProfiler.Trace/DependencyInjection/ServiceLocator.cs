using System;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.DependencyInjection.Extensions;
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

        private void RegisterServices(ServiceCollection serviceCollection)
        {
            serviceCollection.Add(ServiceDescriptor.Singleton(typeof(ITracer), GlobalTracer.Instance));
            serviceCollection.AddSingleton<WrapperService>();
            var types = typeof(WrapperService).Assembly.GetTypes();
            foreach (var type in types)
            {
                if (typeof(IWrapper).IsAssignableFrom(type) && type.IsClass && !type.IsAbstract)
                {
                    serviceCollection.Add(ServiceDescriptor.Singleton(typeof(IWrapper), type));
                }
            }
        }
    }
}
