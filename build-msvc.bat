:: BSD 2-Clause License
::
:: Copyright (c) 2021-2022, Christoph Neuhauser, Felix Brendel
:: All rights reserved.
::
:: Redistribution and use in source and binary forms, with or without
:: modification, are permitted provided that the following conditions are met:
::
:: 1. Redistributions of source code must retain the above copyright notice, this
::    list of conditions and the following disclaimer.
::
:: 2. Redistributions in binary form must reproduce the above copyright notice,
::    this list of conditions and the following disclaimer in the documentation
::    and/or other materials provided with the distribution.
::
:: THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
:: AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
:: IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
:: DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
:: FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
:: DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
:: SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
:: CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
:: OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
:: OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

@echo off
setlocal
pushd %~dp0

set VSLANG=1033
set run_program=true
set debug=false
set build_dir=".build"
set destination_dir="Shipping"
set vcpkg_triplet="x64-windows"

:loop
IF NOT "%1"=="" (
    IF "%1"=="--do-not-run" (
        SET run_program=false
    )
    IF "%1"=="--debug" (
        SET debug=true
    )
    IF "%1"=="--vcpkg-triplet" (
        SET vcpkg_triplet=%2
        SHIFT
    )
    SHIFT
    GOTO :loop
)

where cmake >NUL 2>&1 || echo cmake was not found but is required to build the program && exit /b 1

:: Creates a string with, e.g., -G "Visual Studio 17 2022".
:: Needs to be run from a Visual Studio developer PowerShell or command prompt.
if defined VCINSTALLDIR (
    set VCINSTALLDIR_ESC=%VCINSTALLDIR:\=\\%
)
if defined VCINSTALLDIR (
    set "x=%VCINSTALLDIR_ESC:Microsoft Visual Studio\\=" & set "VsPathEnd=%"
)
if defined VCINSTALLDIR (
    set cmake_generator=-G "Visual Studio %VisualStudioVersion:~0,2% %VsPathEnd:~0,4%"
)
if not defined VCINSTALLDIR (
    set cmake_generator=
)

IF "%VULKAN_SDK%"=="" (
  for /D %%F in (C:\VulkanSDK\*) do (
    set VULKAN_SDK=%%F
    goto vulkan_finished
  )
)
:vulkan_finished


set third_party_dir=%~dp0/third_party
set cmake_args=-DCMAKE_TOOLCHAIN_FILE="third_party/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
               -DVCPKG_TARGET_TRIPLET=%vcpkg_triplet% ^
               -DCMAKE_CXX_FLAGS="/MP" ^
               -Dsgl_DIR="third_party/sgl/install/lib/cmake/sgl/"

set cmake_args_general=-DCMAKE_TOOLCHAIN_FILE="%third_party_dir%/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
               -DVCPKG_TARGET_TRIPLET=%vcpkg_triplet%

if not exist .\third_party\ mkdir .\third_party\
pushd third_party

if not exist .\vcpkg (
   echo ------------------------
   echo    fetching vcpkg
   echo ------------------------
   git clone --depth 1 https://github.com/Microsoft/vcpkg.git || exit /b 1
   call vcpkg\bootstrap-vcpkg.bat -disableMetrics             || exit /b 1
   vcpkg\vcpkg install --triplet=%vcpkg_triplet%              || exit /b 1
)

if not exist .\sgl (
   echo ------------------------
   echo      fetching sgl
   echo ------------------------
   git clone --depth 1 https://github.com/chrismile/sgl.git   || exit /b 1
)

if not exist .\sgl\install (
   echo ------------------------
   echo      building sgl
   echo ------------------------
   mkdir sgl\.build 2> NUL
   pushd sgl\.build

   cmake .. %cmake_generator% %cmake_args_general% ^
            -DCMAKE_INSTALL_PREFIX="%third_party_dir%/sgl/install" -DCMAKE_CXX_FLAGS="/MP" || exit /b 1
   if x%vcpkg_triplet:release=%==x%vcpkg_triplet% (
      cmake --build . --config Debug   -- /m            || exit /b 1
      cmake --build . --config Debug   --target install || exit /b 1
   )
   if x%vcpkg_triplet:debug=%==x%vcpkg_triplet% (
      cmake --build . --config Release -- /m            || exit /b 1
      cmake --build . --config Release --target install || exit /b 1
   )

   popd
)


popd

if %debug% == true (
   echo ------------------------
   echo   building in debug
   echo ------------------------

   set cmake_config="Debug"
) else (
   echo ------------------------
   echo   building in release
   echo ------------------------

   set cmake_config="Release"
)

echo ------------------------
echo       generating
echo ------------------------


cmake %cmake_generator% %cmake_args% -S . -B %build_dir%

echo ------------------------
echo       compiling
echo ------------------------
cmake --build %build_dir% --config %cmake_config% -- /m || exit /b 1

echo ------------------------
echo    copying new files
echo ------------------------
if %debug% == true (
   if not exist %destination_dir%\*.pdb (
      del %destination_dir%\*.dll
   )
   robocopy %build_dir%\Debug\  %destination_dir%  >NUL
   robocopy third_party\sgl\.build\Debug %destination_dir% *.dll *.pdb >NUL
) else (
   if exist %destination_dir%\*.pdb (
      del %destination_dir%\*.dll
      del %destination_dir%\*.pdb
   )
   robocopy %build_dir%\Release\ %destination_dir% >NUL
   robocopy third_party\sgl\.build\Release %destination_dir% *.dll >NUL
)

echo.
echo All done!

pushd %destination_dir%

if %run_program% == true (
   PixelSyncOIT.exe
) else (
   echo Build finished.
)
