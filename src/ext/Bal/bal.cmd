@setlocal
@pushd %~dp0

@set _C=Debug
@set _L=%~dp0..\..\..\build\logs

:parse_args
@if /i "%1"=="release" set _C=Release
@if /i "%1"=="inc" set _INC=1
@if /i "%1"=="clean" set _CLEAN=1
@if not "%1"=="" shift & goto parse_args

@set _B=%~dp0..\..\..\build\Bal.wixext\%_C%

:: Clean

@if "%_INC%"=="" call :clean
@if NOT "%_CLEAN%"=="" goto :end

@echo Building ext\BootstrapperApplications %_C% using %_N%

:: Restore

:: Build
msbuild -Restore -p:Configuration=%_C% -tl -nologo -m -warnaserror test\WixToolsetTest.BootstrapperApplications\WixToolsetTest.BootstrapperApplications.csproj -bl:%_L%\ext_bal_build.binlog || exit /b

msbuild -Restore -p:Configuration=%_C% -tl -nologo -m -warnaserror test\examples\examples.proj -bl:%_L%\bal_examples_build.binlog  || exit /b

:: Test
dotnet test ^
  %_B%\net6.0\WixToolsetTest.BootstrapperApplications.dll ^
  --nologo -l "trx;LogFileName=%_L%\TestResults\bal.wixext.trx" || exit /b

:: Pack
msbuild -t:Pack -p:Configuration=%_C% -tl -nologo -warnaserror -p:NoBuild=true wixext\WixToolset.BootstrapperApplications.wixext.csproj || exit /b
msbuild -t:Pack -Restore -p:Configuration=%_C% -tl -nologo -warnaserror wixext-backward-compatible\WixToolset.Bal.wixext.csproj || exit /b

@goto :end

:clean
@rd /s/q "..\..\..\build\Bal.wixext" 2> nul
@del "..\..\..\build\artifacts\WixToolset.Bal.wixext.*.nupkg" 2> nul
@del "..\..\..\build\artifacts\WixToolset.BootstrapperApplications.wixext.*.nupkg" 2> nul
@del "%_L%\ext_bal_build.binlog" 2> nul
@del "%_L%\TestResults\bal.wixext.trx" 2> nul
@rd /s/q "%USERPROFILE%\.nuget\packages\wixtoolset.bal.wixext" 2> nul
@rd /s/q "%USERPROFILE%\.nuget\packages\wixtoolset.bootstrapperapplications.wixext" 2> nul
@exit /b

:end
@popd
@endlocal
