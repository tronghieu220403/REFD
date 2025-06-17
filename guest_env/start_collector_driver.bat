net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -Command "Start-Process '%~f0' -Verb runAs"
)
fltmc unload EventCollectorDriver
del /f C:\Windows\System32\drivers\EventCollectorDriver.sys
copy /y E:\hieunt210330\EventCollectorDriver.sys C:\Windows\System32\drivers\EventCollectorDriver.sys
fltmc load EventCollectorDriver
sc query EventCollectorDriver