CoreCLR Profiler ILReWrite
==========================================

This sample shows il rewrite (open dev.cmd to develop)

1.hook il in premain assembly.customloadfrom(you profiler C# dll)

2.add try catch finally in to need trace method

Prerequisites
-------------

* CoreCLR Repository (build from source) Dependencies
* Visual Studio 2017 (Windows)
* CLang3.9 (Linux)
* Vcpkg (Windows Linux)

Building
-------------------------

### Build

#### windows 

powershell ./scripts/install-vcpkgs.ps1

```batch
cd "D:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build"
d:
vcvars64
cd D:\ClrProfiler.Trace\src\ClrProfiler
SET BuildArch=x64
SET BuildType=Debug
build
```

#### linux

```batch
cd ~/vcpkg
./vcpkg install spdlog

cd ~/ClrProfiler.Trace/src/ClrProfiler
mkdir -p build
cd build 
cmake ..
make

```

### Setup

```batch
SET CORECLR_PROFILER={cf0d821e-299b-5307-a3d8-b283c03916dd}
SET CORECLR_ENABLE_PROFILING=1
SET CORECLR_PROFILER_PATH=%~dp0\src\ClrProfiler\x64\Debug\ClrProfiler.dll

SET COR_PROFILER={cf0d821e-299b-5307-a3d8-b283c03916dd}
SET COR_ENABLE_PROFILING=1
SET COR_PROFILER_PATH=%~dp0\src\ClrProfiler\x64\Debug\ClrProfiler.dll

: just a sample because ClrProfiler.Trace.dll in this path
SET CLRPROFILER_HOME=%~dp0\src\ClrProfiler.Trace\bin\Debug\netstandard2.0

: net framework need add gac "C:\Program Files (x86)\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.6.1 Tools\x64\gacutil.exe" /i %~dp0\src\ClrProfiler.Trace\bin\Debug\net461\ClrProfiler.Trace.dll

cd src/ClrProfiler.Trace
dotnet publish -f netstandard2.0 -o bin\Debug\netstandard2.0

run you program
```

Help Links:
-------------

https://www.ecma-international.org/publications/files/ECMA-ST/ECMA-335.pdf

https://github.com/Microsoft/clr-samples

https://github.com/brian-reichle/MethodCheck

https://www.codeproject.com/articles/42655/%2fArticles%2f42655%2fNET-file-format-Signatures-under-the-hood-Part-2#LocalVarSig1.1

https://github.com/DataDog/dd-trace-dotnet
