using System;
using System.IO;
using System.Reflection;

namespace ClrProfiler.Trace
{
    public delegate void EndMethodDelegate(object returnValue, Exception ex);

    public class TraceAgent
    {
        private static readonly TraceAgent Instance = new TraceAgent();
        private readonly WrapperService _wrapperService;

        private TraceAgent()
        {
            _wrapperService = new WrapperService();

            AppDomain.CurrentDomain.AssemblyResolve += CurrentDomain_AssemblyResolve;
        }

        private Assembly CurrentDomain_AssemblyResolve(object sender, ResolveEventArgs args)
        {
            if (args.RequestingAssembly.FullName.Contains("ClrProfiler.Trace"))
            {
                Console.WriteLine(args.Name);
                var home = Environment.GetEnvironmentVariable("CLRPROFILER_HOME");
                if (!string.IsNullOrEmpty(home))
                {
                    var filepath = Path.Combine(home, $"{new AssemblyName(args.Name).Name}.dll");
                    if (File.Exists(filepath))
                    {
                        return Assembly.LoadFrom(filepath);
                    }
                }
            }
            return null;
        }

        public static object GetInstance()
        {
            return Instance;
        }

        public object BeforeMethod(string typeName, string methodName, object invocationTarget, object[] methodArguments, UInt32 functionToken)
        {
            try
            {
                var args = methodArguments;
                var endMethodDelegate = this._wrapperService.BeforeWrappedMethod(typeName, methodName, invocationTarget, args, functionToken);
                return endMethodDelegate != null ? new MethodTrace(endMethodDelegate) : default(MethodTrace);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Trace.WriteLine(ex);
                return default(MethodTrace);
            }
        }
    }

    public class TraceMethodInfo
    {
        public string TypeName { get; set; }

        public string MethodName { get; set; }

        public object InvocationTarget { get; set; }

        public object[] MethodArguments { get; set; }

        public UInt32 FunctionToken { get; set; }
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
