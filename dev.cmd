@echo off

SET CORECLR_PROFILER={cf0d821e-299b-5307-a3d8-b283c03916dd}
SET CORECLR_ENABLE_PROFILING=1
SET CORECLR_PROFILER_PATH=%~dp0\ProfilingAPI\ClrProfiler\x64\Debug\ClrProfiler.dll
SET DOTNET_ADDITIONAL_DEPS=%PROGRAMFILES%\dotnet\x64\additionalDeps\Datadog.Trace.ClrProfiler.Managed

echo Starting Visual Studio...
IF EXIST "D:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\IDE\devenv.exe" (
    START "Visual Studio" "D:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\IDE\devenv.exe" "%~dp0\ProfilingAPI\ClrProfiler.sln"
) ELSE (
    START "Visual Studio" "D:\Program Files (x86)\Microsoft Visual Studio\Preview\Enterprise\Common7\IDE\devenv.exe" "%~dp0\ProfilingAPI\ClrProfiler.sln"
)

pause
