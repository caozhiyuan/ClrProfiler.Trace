using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.Specialized;
using OpenTracing.Propagation;

namespace ClrProfiler.Trace.AspNet
{
    internal sealed class RequestHeadersExtractAdapter : ITextMap
    {
        private readonly NameValueCollection _headers;

        public RequestHeadersExtractAdapter(NameValueCollection headers)
        {
            _headers = headers ?? throw new ArgumentNullException(nameof(headers));
        }

        public void Set(string key, string value)
        {
            throw new NotSupportedException("This class should only be used with ITracer.Extract");
        }

        public IEnumerator<KeyValuePair<string, string>> GetEnumerator()
        {
            foreach (var key in _headers.AllKeys)
            {
                yield return new KeyValuePair<string, string>(key, _headers[key]);
            }
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }
    }
}
