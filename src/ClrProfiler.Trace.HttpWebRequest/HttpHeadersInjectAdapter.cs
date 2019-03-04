using System;
using System.Collections;
using System.Collections.Generic;
using System.Net;
using OpenTracing.Propagation;

namespace ClrProfiler.Trace.HttpWebRequest
{
    internal sealed class HttpHeadersInjectAdapter : ITextMap
    {
        private readonly WebHeaderCollection _headers;

        public HttpHeadersInjectAdapter(WebHeaderCollection headers)
        {
            _headers = headers ?? throw new ArgumentNullException(nameof(headers));
        }

        public void Set(string key, string value)
        {
            _headers[key] = value;
        }

        public IEnumerator<KeyValuePair<string, string>> GetEnumerator()
        {
            throw new NotSupportedException("This class should only be used with ITracer.Inject");
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }
    }
}
