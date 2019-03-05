
using System;
using System.Threading;
using OpenTracing;

#if NET
using System.Runtime.Remoting;
using System.Web;
using System.Runtime.Remoting.Messaging;
#endif

namespace ClrProfiler.Trace
{
    public class TraceScopeManager : IScopeManager
    {
#if NET        
        private readonly string _logicalDataKey = "__AsyncLocalScope_Current__" + Guid.NewGuid().ToString("D");

        public IScope Active
        {
            get
            {
                return (CallContext.LogicalGetData(this._logicalDataKey) as ObjectHandle)?.Unwrap() as IScope;
            }
            set
            {
                CallContext.LogicalSetData(this._logicalDataKey, new ObjectHandle(value));
            }
        }
#else
        private readonly AsyncLocal<IScope> _current = new AsyncLocal<IScope>();

        public IScope Active
        {
            get { return this._current.Value; }
            set { this._current.Value = value; }
        }
#endif
        public IScope Activate(ISpan span, bool finishSpanOnDispose)
        {
            return new AsyncLocalScope(this, span, finishSpanOnDispose);
        }

        private class AsyncLocalScope : IScope, IDisposable
        {
            private readonly TraceScopeManager _scopeManager;
            private readonly ISpan _wrappedSpan;
            private readonly bool _finishOnDispose;
            private readonly IScope _scopeToRestore;

            public AsyncLocalScope(
                TraceScopeManager scopeManager,
                ISpan wrappedSpan,
                bool finishOnDispose)
            {
                this._scopeManager = scopeManager;
                this._wrappedSpan = wrappedSpan;
                this._finishOnDispose = finishOnDispose;
                this._scopeToRestore = scopeManager.Active;
                scopeManager.Active = this;
            }

            public ISpan Span => this._wrappedSpan;

            public void Dispose()
            {
                // no check sometimes AsyncLocal not work
                //if (this._scopeManager.Active != this)
                //    return;

                if (this._finishOnDispose)
                    this._wrappedSpan.Finish();
                this._scopeManager.Active = this._scopeToRestore;
            }
        }
    }
}
