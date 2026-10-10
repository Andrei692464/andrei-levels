@echo off
title Actualizare mod Andrei123459 Levels

rem === Daca jocul tau e in alt loc, schimba calea de mai jos ===
set "MODS=C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\geode\mods"

set "URL=https://github.com/Andrei692464/andrei-levels/releases/latest/download/andrei123459.levels.geode"
set "TMPFILE=%TEMP%\andrei123459.levels.geode"

echo.
echo Inchide Geometry Dash inainte de actualizare!
echo.

if not exist "%MODS%" (
  echo Folderul nu exista: %MODS%
  echo Deschide acest fisier cu Notepad si schimbi linia "set MODS=..."
  pause
  exit /b 1
)

echo Descarc ultima versiune...
curl -L -f -s -o "%TMPFILE%" "%URL%"
if errorlevel 1 (
  echo Descarcarea a esuat. Verifica daca build-ul de pe GitHub s-a terminat cu bifa verde.
  pause
  exit /b 1
)

copy /Y "%TMPFILE%" "%MODS%\andrei123459.levels.geode" >nul
if errorlevel 1 (
  echo Nu am putut copia fisierul. Inchide jocul sau ruleaza ca Administrator.
  pause
  exit /b 1
)

del "%TMPFILE%" >nul 2>&1
echo.
echo Gata! Modul a fost actualizat. Poti porni jocul.
pause
