@echo off

SET CORECLR_PROFILER={cf0d821e-299b-5307-a3d8-b283c03916dd}
SET CORECLR_ENABLE_PROFILING=1
SET CORECLR_PROFILER_PATH=%~dp0\ProfilingAPI\ClrProfiler\x64\Debug\ClrProfiler.dll

SET COR_PROFILER={cf0d821e-299b-5307-a3d8-b283c03916dd}
SET COR_ENABLE_PROFILING=1
SET COR_PROFILER_PATH=%~dp0\ProfilingAPI\ClrProfiler\x64\Debug\ClrProfiler.dll

: just a sample because ClrProfiler.Trace.dll in this path
SET CLRPROFILER_HOME=%~dp0\ProfilingAPI\ClrProfiler.Trace\bin\Debug\netstandard2.0

: net framework need add gac "C:\Program Files (x86)\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.6.1 Tools\x64\gacutil.exe" /i %~dp0\ProfilingAPI\ClrProfiler.Trace\bin\Debug\net461\ClrProfiler.Trace.dll

echo Starting Visual Studio...
IF EXIST "D:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\IDE\devenv.exe" (
    START "Visual Studio" "D:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\IDE\devenv.exe" "%~dp0\ProfilingAPI\ClrProfiler.sln"
) ELSE (
    START "Visual Studio" "D:\Program Files (x86)\Microsoft Visual Studio\Preview\Enterprise\Common7\IDE\devenv.exe" "%~dp0\ProfilingAPI\ClrProfiler.sln"
)
