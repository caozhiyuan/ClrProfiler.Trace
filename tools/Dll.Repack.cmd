
SET BuildType=%1
if "%BuildType%"=="Release" (SET BuildType=Release) else (SET BuildType=Debug)

SET OUTDLL=ClrProfiler.Trace.dll
SET DLLS=ClrProfiler.Trace.dll Jaeger.dll Jaeger.Thrift.dll Jaeger.Thrift.VendoredThrift.dll Newtonsoft.Json.dll
SET DLLS=%DLLS% Microsoft.Extensions.DependencyInjection.Abstractions.dll Microsoft.Extensions.DependencyInjection.dll 
SET DLLS=%DLLS% Microsoft.CSharp.dll Microsoft.Extensions.Logging.Abstractions.dll

cd ../src/ClrProfiler.Trace
dotnet publish -c %BuildType% -f netstandard2.0 -o bin\%BuildType%\netstandard2.0

cd bin\%BuildType%\netstandard2.0

%~dp0\ILRepack.exe /keyfile:%~dp0\clrprofiler.snk /ver:1.0.0 /copyattrs /xmldocs /ndebug /internalize /out:%OUTDLL% %DLLS%