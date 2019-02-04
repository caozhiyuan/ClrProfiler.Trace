cd /d %~dp0

SET BuildType=%1
if "%BuildType%"=="Release" (SET BuildType=Release) else (SET BuildType=Debug)

SET BuildFramework=%2
if "%BuildFramework%"=="net461" (SET BuildFramework=net461) else (SET BuildFramework=netstandard2.0)

SET WorkDir=%~dp0

SET OUTDLL=ClrProfiler.Trace.dll
SET DLLS=ClrProfiler.Trace.dll Jaeger.dll Jaeger.Thrift.dll Jaeger.Thrift.VendoredThrift.dll Newtonsoft.Json.dll
SET DLLS=%DLLS% Microsoft.Extensions.DependencyInjection.Abstractions.dll Microsoft.Extensions.DependencyInjection.dll 
SET DLLS=%DLLS% Microsoft.Extensions.Logging.Abstractions.dll

cd ../src
dotnet build -c %BuildType%

cd ClrProfiler.Trace
dotnet publish -c %BuildType% -f %BuildFramework% -o bin\%BuildType%\%BuildFramework%

cd bin\%BuildType%\%BuildFramework%

%WorkDir%ILRepack.exe /keyfile:%~dp0\clrprofiler.snk /ver:1.0.0 /copyattrs /xmldocs /ndebug /internalize /out:%OUTDLL% %DLLS%


if "%BuildFramework%"=="net461" (
  %WorkDir%gacutil.exe /uf ClrProfiler.Trace
  %WorkDir%gacutil.exe /i ClrProfiler.Trace.dll
  %WorkDir%gacutil.exe /i OpenTracing.dll
  %WorkDir%gacutil.exe /i netstandard.dll
)

cd /d %~dp0
