@echo off
setlocal
cd /d "%~dp0"
set "W64=C:\Programlama Dili\w64devkit"

if not exist "%W64%\bin\gcc.exe" (
  echo HATA: gcc bulunamadi: %W64%\bin\gcc.exe
  echo build.bat icindeki W64 yolunu duzelt.
  pause
  exit /b 1
)
set "PATH=%W64%\bin;%PATH%"

echo antitroll.dll derleniyor...
gcc -O2 -shared -static-libgcc -Iminhook\include -o antitroll.dll antitroll.c minhook\src\buffer.c minhook\src\hook.c minhook\src\trampoline.c minhook\src\hde\hde64.c -s
if errorlevel 1 goto fail
echo injector.exe derleniyor...
gcc -O2 -o injector.exe injector.c -s
if errorlevel 1 goto fail

echo.
echo Derleme tamam.
if /I not "%~1"=="nopause" pause
exit /b 0

:fail
echo.
echo HATA: derleme basarisiz. Among Us aciksa kapatip tekrar dene (DLL kilitli olabilir).
pause
exit /b 1
