using System;
using System.IO;
using System.Reflection;

namespace ClrProfiler.Trace
{
    public class AssemblyResolver
    {
        public static readonly AssemblyResolver Instance = new AssemblyResolver();

        private AssemblyResolver()
        {
            AppDomain.CurrentDomain.AssemblyResolve += CurrentDomain_AssemblyResolve;
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

        public void Init()
        {

        }
    }
}
