using System;
using System.Threading.Tasks;
using ClrProfiler.Trace.Extensions;
using Microsoft.AspNetCore.Http;
using OpenTracing;
using OpenTracing.Propagation;
using OpenTracing.Tag;

namespace ClrProfiler.Trace.AspNetCore
{
    public class ProfilerMiddleWare
    {
        private readonly RequestDelegate _next;

        private readonly ITracer _tracer;

        public ProfilerMiddleWare(RequestDelegate next, ITracer tracer)
        {

            _tracer = tracer;
            _next = next;
        }

        public async Task Invoke(HttpContext context)
        {
            var tracer = _tracer;

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
