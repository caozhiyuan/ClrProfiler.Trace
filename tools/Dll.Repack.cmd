@echo off
:: BatchGotAdmin
:-------------------------------------
REM  --> Check for permissions
    IF "%PROCESSOR_ARCHITECTURE%" EQU "amd64" (
>nul 2>&1 "%SYSTEMROOT%\SysWOW64\cacls.exe" "%SYSTEMROOT%\SysWOW64\config\system"
) ELSE (
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"
)

REM --> If error flag set, we do not have admin.
if '%errorlevel%' NEQ '0' (
    echo Requesting administrative privileges...
    goto UACPrompt
) else ( goto gotAdmin )

:UACPrompt
    echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
    set params= %*
    echo UAC.ShellExecute "cmd.exe", "/c ""%~s0"" %params:"=""%", "", "runas", 1 >> "%temp%\getadmin.vbs"

    "%temp%\getadmin.vbs"
    del "%temp%\getadmin.vbs"
    exit /B

:gotAdmin
    pushd "%CD%"
    CD /D "%~dp0"
:--------------------------------------    

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
