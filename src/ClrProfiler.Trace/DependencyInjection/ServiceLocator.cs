using System;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.DependencyInjection.Extensions;
using OpenTracing;
using OpenTracing.Util;

namespace ClrProfiler.Trace.DependencyInjection
{
    public class ServiceLocator
    {
        private static readonly IServiceProvider ServiceProvider;

        static ServiceLocator()
        {
            var serviceCollection = new ServiceCollection();

            RegisterServices(serviceCollection);

            ServiceProvider = serviceCollection.BuildServiceProvider();
        }

        public static T GetService<T>()
        {
            return (T)ServiceProvider.GetService(typeof(T));
        }

        private static void RegisterServices(ServiceCollection serviceCollection)
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
