@echo off
setlocal enabledelayedexpansion

rem Remove mtllib lines from all .obj files
for /r %%f in (*.obj) do (
    echo Cleaning: %%f
    findstr /v /b "mtllib " "%%f" > "%%f.tmp"
    move /y "%%f.tmp" "%%f" > nul
)

rem Delete all .mtl files
for /r %%f in (*.mtl) do (
    echo Deleting: %%f
    del "%%f"
)

set "SRCDIR=%CD%"
set "OUTDIR=..\GameData\Meshes"

for /r %%f in (*.obj) do (
    set "FULLPATH=%%~dpf"
    set "RELPATH=!FULLPATH:%SRCDIR%\=!"
        
    echo Converting: %%f
    ..\Tools\meshconvert -ft cmo -y -t -o "%OUTDIR%\!RELPATH!%%~nf.cmo" "%%f"
)