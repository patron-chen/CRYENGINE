@echo off
pushd "%~dp0\..\.."
mkdir solutions_cmake\android > nul 2>&1
cd solutions_cmake\android
..\..\Tools\CMake\msvc-android-cmake\3.6.1\bin\cmake -G "Visual Studio 14 2015 ARM" -D CMAKE_TOOLCHAIN_FILE=Tools\CMake\toolchain\android\Android-MSVC.cmake ..\..
if ERRORLEVEL 1 (
	pause
) else (
	echo Starting cmake-gui...
	start ..\..\Tools\CMake\msvc-android-cmake\3.6.1\bin\cmake-gui .
)
popd
