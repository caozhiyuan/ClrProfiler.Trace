CoreCLR Profiler ILReWrite
==========================================

This sample shows il rewrite (open dev.cmd to develop)

1¡¢hook il in premain assembly.customloadfrom(you profiler C# dll)
2¡¢add try catch finally in to need trace method

Prerequisites
-------------

* CoreCLR Repository (build from source) Dependencies
* Visual Studio 2017

Building
-------------------------

### Build

Build ClrProfiler.dll

### Setup

```batch
SET CORECLR_PROFILER={cf0d821e-299b-5307-a3d8-b283c03916dd}
SET CORECLR_ENABLE_PROFILING=1
SET CORECLR_PROFILER_PATH=C:\filePath\to\ClrProfiler.dll
corerun YourProgram.dll
```

Help Links:
-------------

https://www.ecma-international.org/publications/files/ECMA-ST/ECMA-335.pdf

https://github.com/DataDog/dd-trace-dotnet

https://github.com/Microsoft/clr-samples

https://github.com/brian-reichle/MethodCheck

https://www.codeproject.com/articles/42655/%2fArticles%2f42655%2fNET-file-format-Signatures-under-the-hood-Part-2#LocalVarSig1.1
