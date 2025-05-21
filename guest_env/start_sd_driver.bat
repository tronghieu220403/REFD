net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -Command "Start-Process '%~f0' -Verb runAs"
)
fltmc unload SelfDefenseKernel
del /f C:\Windows\System32\drivers\SelfDefenseKernel.sys
copy /y E:\SelfDefenseKernel.sys C:\Windows\System32\drivers\SelfDefenseKernel.sys
InfDefaultInstall.exe E:\SelfDefenseKernel.inf
fltmc load SelfDefenseKernel
sc query SelfDefenseKernel