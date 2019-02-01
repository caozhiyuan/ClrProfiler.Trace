using System;
using Microsoft.Extensions.DependencyInjection;

namespace ClrProfiler.Trace.DependencyInjection
{
    public class ServiceLocator
    {
        private IServiceProvider _serviceProvider;

        public static readonly ServiceLocator Instance = new ServiceLocator();

        public T GetService<T>()
        {
            return (T)_serviceProvider.GetService(typeof(T));
        }

        public void RegisterServices(Action<ServiceCollection> action)
        {
            if (action != null)
            {
                var serviceCollection = new ServiceCollection();
                action(serviceCollection);
                _serviceProvider = serviceCollection.BuildServiceProvider();
            }
        }
    }
}
