#!/bin/sh

export CORECLR_PROFILER={cf0d821e-299b-5307-a3d8-b283c03916dd}
export CORECLR_ENABLE_PROFILING=1
export CORECLR_PROFILER_PATH=~/ClrProfiler.Trace/src/ClrProfiler/build/ClrProfiler.so
export CLRPROFILER_HOME=~/ClrProfiler.Trace/src/ClrProfiler.Trace/bin/Debug/netstandard2.0

code
