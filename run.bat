@echo off
setlocal
cd /d "%~dp0"
title Among Us Anti-Troll

set "GAME=Among Us.exe"
set "STEAMID=945360"

if not exist "antitroll.dll" call build.bat nopause
if not exist "injector.exe" call build.bat nopause
if not exist "antitroll.dll" exit /b 1

tasklist /FI "IMAGENAME eq %GAME%" 2>nul | find /I "%GAME%" >nul
if errorlevel 1 (
  echo Oyun kapali, Steam'den baslatiliyor. Steam kullanmiyorsan oyunu elle ac.
  start "" steam://rungameid/%STEAMID%
)
echo Among Us bekleniyor...
:wait
tasklist /FI "IMAGENAME eq %GAME%" 2>nul | find /I "%GAME%" >nul
if errorlevel 1 (
  timeout /t 2 /nobreak >nul
  goto wait
)
echo Oyun acildi, 10 saniye bekleniyor...
timeout /t 10 /nobreak >nul

injector.exe
if errorlevel 1 (
  pause
  exit /b 1
)

echo Hook kuruluyor (en fazla 60 sn)...
set /a n=0
:logwait
findstr /C:"hazir" /C:"HATA" antitroll_log.txt >nul 2>&1
if errorlevel 1 (
  set /a n+=1
  if %n% GEQ 30 goto showlog
  timeout /t 2 /nobreak >nul
  goto logwait
)
:showlog
echo.
type antitroll_log.txt
echo.
echo Mac sirasinda olaylar: %~dp0antitroll_log.txt
echo Sure ayari: antitroll.ini (first_round_seconds)
pause
