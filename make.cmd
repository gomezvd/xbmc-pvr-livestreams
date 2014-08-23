@echo off
setlocal enableextensions enabledelayedexpansion

rem ****************************************************************************
rem * Parameters
rem ****************************************************************************

set project_dir=project\VS2013
set project_name=xbmc-pvr-livestreams
set build_config=Release

set msbuild="%VS120COMNTOOLS%..\..\..\MSBuild\12.0\bin\MSBuild.exe"
set zip="project\BuildDependencies\bin\7za.exe"

rem ****************************************************************************
rem * Call rules
rem ****************************************************************************

if "%*" == "" (
    call :all
) else (
    for %%x in (%*) do call :%%x
)
goto :eof

rem ****************************************************************************
rem * all
rem ****************************************************************************

:all
call :dist-zip
goto :eof

rem ****************************************************************************
rem * build
rem ****************************************************************************

:build
%msbuild% %project_dir%\%project_name%.sln /t:Build /p:Configuration="%build_config%"
goto :eof

rem ****************************************************************************
rem * rebuild
rem ****************************************************************************

:rebuild
call :clean
call :build
goto :eof

rem ****************************************************************************
rem * clean
rem ****************************************************************************

:clean
%msbuild% %project_dir%\%project_name%.sln /t:Clean /p:Configuration="%build_config%"

del addons\pvr.livestreams\XBMC_LiveStreams_PVR_win32.dll 2>nul
rd /q /s project\VS2013\platform\Debug 2>nul
rd /q /s project\VS2013\platform\Release 2>nul
rd /q /s project\VS2013\livestreamspvr\Debug 2>nul
rd /q /s project\VS2013\livestreamspvr\Release 2>nul
del /q %project_dir%\%project_name%.sdf >nul 2>nul
del /q /a:h %project_dir%\%project_name%.v12.suo >nul 2>nul
goto :eof

rem ****************************************************************************
rem * distr-zip
rem ****************************************************************************

:dist-zip
call :rebuild

if exist addons\pvr.livestreams\changelog.txt del addons\pvr.livestreams\changelog.txt > nul
if exist addons\pvr.livestreams\addon.xml del addons\pvr.livestreams\addon.xml > nul

REM ------- Grab version from configure.ac into addon.xml file--------

setlocal enableextensions enabledelayedexpansion

for /f "usebackq delims=" %%i in ("configure.ac") do (

	set "zeile=%%i"

	set zeile=!zeile:~11,30!
	set zeile1=!zeile:~0,5!
	IF !zeile1!==MAJOR (
		set maj=!zeile:~8,30!
		set maj=!maj:~0,-1!
		)
	IF !zeile1!==MINOR (
		set min=!zeile:~8,30!
		set min=!min:~0,-1!
		)
	IF !zeile1!==MICRO ( 
		set mic=!zeile:~8,30!
		set mic=!mic:~0,-1!
		echo !maj!.!min!.!mic!>>test.tmp
		)
	)

for /F "delims=" %%i in (test.tmp) do set "zeile=%%i"
set "Von=  version" 
set "Nach=  version="X"" 
set Nach=!Nach:X=%zeile%!

for /f "usebackq delims=" %%i in ("addons/pvr.livestreams/addon.xml.in") do (
	set "Line=%%i"
	set Line=!Line:%Von%=%Nach%!
	set Line1=!Line!
	set line1=!line1:~0,9!
	set line1=!line1:~2,7!
	if !Line1!==version set Line=!Line:~0,-12!
	echo !Line! >> addons/pvr.livestreams/addon.xml 
	)

setlocal disabledelayedexpansion 

copy changelog.txt addons\pvr.livestreams\changelog.txt
copy %project_dir%\livestreamspvr\%build_config%\XBMC_LiveStreams_PVR_win32.dll addons\pvr.livestreams

set zipfile=pvr.livestreams.%maj%.%min%.%mic%.zip

if exist %zipfile% del %zipfile% > NUL

%zip% a %zipfile% .\addons\pvr.livestreams -xr!*.in -xr!*.am -xr!.gitignore >nul

if exist test.tmp del test.tmp

goto :eof
