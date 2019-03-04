cd /d %~dp0

SET BuildType=%1
if "%BuildType%"=="Release" (SET BuildType=Release) else (SET BuildType=Debug)

SET BuildFramework=%2
if "%BuildFramework%"=="net461" (SET BuildFramework=net461) else (SET BuildFramework=netstandard2.0)

if "%BuildFramework%"=="net461" (
  cacls.exe "%SystemDrive%\System Volume Information" >nul 2>nul
  if %errorlevel%==0 goto Admin
  if exist "%temp%\getadmin.vbs" del /f /q "%temp%\getadmin.vbs"
  echo Set RequestUAC = CreateObject^("Shell.Application"^)>"%temp%\getadmin.vbs"
  echo RequestUAC.ShellExecute "%~s0","","","runas",1 >>"%temp%\getadmin.vbs"
  echo WScript.Quit >>"%temp%\getadmin.vbs"
  "%temp%\getadmin.vbs" /f
  if exist "%temp%\getadmin.vbs" del /f /q "%temp%\getadmin.vbs"
  exit

  :Admin
  echo got Administrator
)

SET WorkDir=%~dp0

SET OUTDLL=ClrProfiler.Trace.dll
SET DLLS=ClrProfiler.Trace.dll Jaeger.dll Jaeger.Thrift.dll Jaeger.Thrift.VendoredThrift.dll Newtonsoft.Json.dll
SET DLLS=%DLLS% Microsoft.Extensions.DependencyInjection.Abstractions.dll Microsoft.Extensions.DependencyInjection.dll 
SET DLLS=%DLLS% Microsoft.Extensions.Logging.Abstractions.dll

set _VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist %_VSWHERE% (
  for /f "usebackq tokens=*" %%i in (`%_VSWHERE% -latest -prerelease -property installationPath`) do set _VSPATH=%%i
)

cd ../src
"%_VSPATH%\MSBuild\15.0\Bin\MSBuild.exe" /p:Configuration="%BuildType%"

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
