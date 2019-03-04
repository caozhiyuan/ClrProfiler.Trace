using System;
using System.Net;
using ClrProfiler.Trace.Extensions;
using ClrProfiler.Trace.Utils;
using OpenTracing;
using OpenTracing.Tag;

namespace ClrProfiler.Trace.HttpWebRequest
{
    public class HttpWebRequestGetResponse : IMethodWrapper
    {
        private const string TypeName = "System.Net.HttpWebRequest";
        private const string AssemblyName = "System";
        private const string MethodName = "GetResponse";

        private readonly ITracer _tracer;

        public HttpWebRequestGetResponse(ITracer tracer)
        {
            _tracer = tracer;
        }

        public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
        {
            var request = traceMethodInfo.InvocationTarget as System.Net.HttpWebRequest;
            if (request == null)
            {
                return null;
            }

            var scope = _tracer.BuildSpan("http.out")
                .WithTag(Tags.SpanKind, Tags.SpanKindClient)
                .WithTag(Tags.Component, "HttpClient")
                .WithTag(Tags.HttpMethod, request.Method)
                .WithTag(Tags.HttpUrl, request.RequestUri.ToString())
                .WithTag(Tags.PeerHostname, request.RequestUri.Host)
                .WithTag(Tags.PeerPort, request.RequestUri.Port)
                .StartActive();

            traceMethodInfo.TraceContext = scope;

            return delegate(object returnValue, Exception ex)
            {
                Leave(traceMethodInfo, returnValue, ex);
            };
        }

        private void Leave(TraceMethodInfo traceMethodInfo, object ret, Exception ex)
        {
            var scope = (IScope)traceMethodInfo.TraceContext;
            if (ex != null)
            {
                scope.Span.SetException(ex);
            }

            var response = (HttpWebResponse)ret;
            if (response != null)
            {
                scope.Span.SetTag(Tags.HttpStatus, (int)response.StatusCode);
            }

            scope.Span.Finish();
        }

        public bool CanWrap(TraceMethodInfo traceMethodInfo)
        {
            var invocationTargetType = traceMethodInfo.Type;
            var assemblyName = invocationTargetType.Assembly.GetName().Name;
            if (assemblyName == AssemblyName && TypeName == invocationTargetType.FullName)
            {
                if (traceMethodInfo.MethodBase.Name == MethodName)
                {
                    return true;
                }
            }

            return false;
        }
    }
}
