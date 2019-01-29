using System;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Http;
using ClrProfiler.Trace.DependencyInjection;
using ClrProfiler.Trace.Extensions;
using OpenTracing;
using OpenTracing.Tag;
using OpenTracing.Propagation;

namespace ClrProfiler.Trace.Wrappers.AspNetCore
{
    public class ProfilerMiddleWare
    {
        private readonly ServiceLocator _serviceLocator;
        private readonly RequestDelegate _next;

        public ProfilerMiddleWare(RequestDelegate next, ServiceLocator serviceLocator)
        {
            _serviceLocator = serviceLocator;
            _next = next;
        }

        public async Task Invoke(HttpContext context)
        {
            var tracer = _serviceLocator.GetService<ITracer>();

            var extractedSpanContext = tracer.Extract(BuiltinFormats.HttpHeaders, new RequestHeadersExtractAdapter(context.Request.Headers));

            var scope = tracer.BuildSpan("http.in")
                .AsChildOf(extractedSpanContext)
                .WithTag(Tags.SpanKind, Tags.SpanKindClient)
                .WithTag(Tags.Component, "AspNetCore")
                .WithTag(Tags.HttpMethod, context.Request.Method.ToString())
                .WithTag(Tags.HttpUrl, context.Request.Path.ToString())
                .WithTag(Tags.PeerHostname, context.Request.Host.Host)
                .WithTag(Tags.PeerPort, context.Request.Host.Port.GetValueOrDefault(0))
                .StartActive();

            await Next(context, scope);
        }

        private async Task Next(HttpContext context, IScope scope)
        {
            try
            {
                await _next(context);

                scope.Span.SetTag(Tags.HttpStatus, context.Response.StatusCode);
            }
            catch (Exception ex)
            {
                scope.Span.SetException(ex);
            }

            scope.Span.Finish();
        }
    }
}
