﻿using System;

namespace Datadog.Trace.ClrProfiler
{
    public delegate void EndMethodDelegate(object returnValue, Exception ex);

    public class TraceAgent
    {
        private static readonly TraceAgent Instance = new TraceAgent();
        private readonly WrapperService _wrapperService;

        private TraceAgent()
        {
            _wrapperService = new WrapperService();
        }

        public static object GetInstance()
        {
            return Instance;
        }

        public object BeforeMethod(string typeName, string methodName, object invocationTarget, object[] methodArguments)
        {
            try
            {
                var args = methodArguments;
                var endMethodDelegate = this._wrapperService.BeforeWrappedMethod(typeName, methodName, invocationTarget, args);
                return ((endMethodDelegate != null) ? new MethodTrace(endMethodDelegate) : null);
            }
            catch (Exception ex)
            {
                return null;
            }
        }
    }

    public class WrapperService
    {
        public EndMethodDelegate BeforeWrappedMethod(string typeName, string methodName, object invocationTarget, object[] methodArguments)
        {
#if DEBUG
            Console.WriteLine($"typeName;{typeName} methodName:{methodName}");
            if (methodArguments != null)
            {
                Console.WriteLine("methodArguments:");
                foreach (var methodArgument in methodArguments)
                {
                    Console.WriteLine(methodArgument);
                }
            }
#endif
            return delegate (object returnValue, Exception ex)
            {
#if DEBUG
                Console.WriteLine($"returnValue:{returnValue}");
                Console.WriteLine($"ex:{ex}");
#endif
            };
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