@echo on
echo Running %0 %*.

set configuration=%1
set platform=%2
set outputfile=%3

rem : Install targets into a Maya module structure.

set modulepath=maya\%configuration%\%MAYA_PREFSVERSION%
mkdir %modulepath%\plug-ins
mkdir %modulepath%\scripts
xcopy /fy %outputfile% %modulepath%\plug-ins
xcopy /fy %platform%\%configuration%\niflib.dll %modulepath%
xcopy /fy *.mel %modulepath%\scripts