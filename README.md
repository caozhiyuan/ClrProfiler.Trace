# CoreCLR Profiler ILReWrite

This sample shows il rewrite (open dev.cmd to develop)

1.hook il in premain assembly.customloadfrom(you profiler C# dll)

2.add try catch finally in to need trace method

## Prerequisites

* CoreCLR Repository (build from source) Dependencies
* Visual Studio 2017 (C++ Required) 
* CLang3.9 (Linux)
* Vcpkg (Windows Linux)
* [WiX Toolset 3.11.1](http://wixtoolset.org/releases/) to build Windows installer (msi)
* [WiX Toolset VS2017 Extension](https://marketplace.visualstudio.com/items?itemName=RobMensching.WixToolsetVisualStudio2017Extension)
  
Building

### Build

#### windows 

```batch

git clone https://github.com/dotnet/coreclr.git
git clone https://github.com/caozhiyuan/ClrProfiler.Trace.git

cd ClrProfiler.Trace
powershell ./scripts/install-vcpkgs.ps1

set _VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist %_VSWHERE% ( for /f "usebackq tokens=*" %i in (`%_VSWHERE% -latest -prerelease -property installationPath`) do set _VSPATH=%i)
call "%_VSPATH%\VC\Auxiliary\Build\vcvars64.bat" 

cd src\ClrProfiler
SET BuildArch=x64
SET BuildType=Debug
build
```

#### linux

```batch

git clone https://github.com/dotnet/coreclr.git
git clone https://github.com/caozhiyuan/ClrProfiler.Trace.git

git clone https://github.com/Microsoft/vcpkg.git
cd ~/vcpkg
./bootstrap-vcpkg.sh
./vcpkg install spdlog

cd ~/ClrProfiler.Trace/src/ClrProfiler
mkdir build
cd build 
cmake ..
make

```

### Setup

```batch

SET CORECLR_PROFILER={cf0d821e-299b-5307-a3d8-b283c03916dd}
SET CORECLR_ENABLE_PROFILING=1
SET CORECLR_PROFILER_PATH=%WorkDir%src\ClrProfiler\x64\Debug\ClrProfiler.dll
SET CORECLR_PROFILER_HOME=%WorkDir%src\ClrProfiler.Trace\bin\Debug\netstandard2.0

SET COR_PROFILER={af0d821e-299b-5307-a3d8-b283c03916dd}
SET COR_ENABLE_PROFILING=1
SET COR_PROFILER_PATH=%WorkDir%src\ClrProfiler\x64\Debug\ClrProfiler.dll
SET COR_PROFILER_HOME=%WorkDir%src\ClrProfiler.Trace\bin\Debug\net461

cd tools
Dll.Repack.cmd Debug 

: or Dll.Repack.cmd Release

download https://github.com/jaegertracing/jaeger/releases/download/v1.9.0/jaeger-1.9.0-windows-amd64.tar.gz
run jaeger-all-in-one.exe

run examples Samples.WebApi

open http://localhost:16686

```

## Notice

### About Async Method Trace

if use scope, you should startActive(false) and restore active scope at method end

it's important if you want get ActiveSpan, code like this :

``` C#
 public EndMethodDelegate BeforeWrappedMethod(TraceMethodInfo traceMethodInfo)
 {
     var dbCommand = (DbCommand)traceMethodInfo.InvocationTarget;
     var scope = _tracer.BuildSpan("mssql.command")
         .WithTag(Tags.SpanKind, Tags.SpanKindClient)
         .WithTag(Tags.Component, "SqlServer")
         .WithTag(Tags.DbInstance, dbCommand.Connection.ConnectionString)
         .WithTag(Tags.DbStatement, dbCommand.CommandText)
         .WithTag(TagMethod, traceMethodInfo.MethodBase.Name)
         .WithTag(TagIsAsync, true)
         .StartActive(false);

     traceMethodInfo.TraceContext = scope;

     return delegate (object returnValue, Exception ex)
     {
         DelegateHelper.AsyncMethodEnd(Leave, traceMethodInfo, ex, returnValue);

         // for async method , at method end restore active scope, important
         var tempScope = (IScope)traceMethodInfo.TraceContext;
         tempScope.Dispose();
     };
 }

 private void Leave(TraceMethodInfo traceMethodInfo, object ret, Exception ex)
 {
     var scope = (IScope)traceMethodInfo.TraceContext;
     if (ex != null)
     {
         scope.Span.SetException(ex);
     }
     scope.Span.Finish();
 }
```

## Help Links:
-------------

https://www.ecma-international.org/publications/files/ECMA-ST/ECMA-335.pdf

https://github.com/Microsoft/clr-samples

https://github.com/brian-reichle/MethodCheck

https://www.codeproject.com/articles/42655/%2fArticles%2f42655%2fNET-file-format-Signatures-under-the-hood-Part-2#LocalVarSig1.1

https://github.com/DataDog/dd-trace-dotnet
