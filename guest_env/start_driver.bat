fltmc unload EventCollectorDriver
del /f C:\Windows\System32\drivers\EventCollectorDriver.sys
copy /y E:\EventCollectorDriver.sys C:\Windows\System32\drivers\EventCollectorDriver.sys
InfDefaultInstall.exe E:\EventCollectorDriver.inf
fltmc load EventCollectorDriver
sc query EventCollectorDriver