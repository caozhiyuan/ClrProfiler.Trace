
SET OUTDLL=ClrProfiler.Trace.dll
SET DLLS=ClrProfiler.Trace.dll OpenTracing.dll Jaeger.dll Jaeger.Thrift.dll Jaeger.Thrift.VendoredThrift.dll Newtonsoft.Json.dll

cd ../src/ClrProfiler.Trace
dotnet publish -f netstandard2.0 -o bin\Debug\netstandard2.0

cd bin\Debug\netstandard2.0

%~dp0\ILRepack.exe /keyfile:%~dp0\clrprofiler.snk /ver:1.0.0 /copyattrs /xmldocs /ndebug /internalize /verbose /out:%OUTDLL% %DLLS%