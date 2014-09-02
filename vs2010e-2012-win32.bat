rem : Launch VS for this specific flavour of Maya

set MAYA_VERSION=2012
set MAYA_PLATFORM=win32
set MAYA_LOCATION=D:\Programs\Autodesk\%MAYA_PLATFORM\Maya%MAYA_VERSION%
set VC="C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE\VCExpress.exe"

set MAYA_PREFSVERSION=%MAYA_VERSION%
%VC% maya_nif_plugin.sln
