;
; Maya Plug-in Self-Installer for Windows
; (NifTools - http://niftools.sourceforge.net) 
; (NSIS - http://nsis.sourceforge.net)
;
; Copyright (c) 2005-2007, NIF File Format Library and Tools
; All rights reserved.
; 
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are
; met:
; 
;     * Redistributions of source code must retain the above copyright
;       notice, this list of conditions and the following disclaimer.
;     * Redistributions in binary form must reproduce the above copyright
;       notice, this list of conditions and the following disclaimer in the
;       documentation ; and/or other materials provided with the
;       distribution.
;     * Neither the name of the NIF File Format Library and Tools project
;       nor the names of its contributors may be used to endorse or promote
;       products derived from this software without specific prior written
;       permission.
; 
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
; IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
; THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
; PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
; CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
; EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
; PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
; LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
; NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
; SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

!include "MUI.nsh"

!define VERSION "1.5.8"
!define MAYA_VERSION "6.5"
!define FULL_NAME "NIF Plug-in ${VERSION} for Maya ${MAYA_VERSION}"
!define MED_NAME "NIF Plug-in for Maya ${MAYA_VERSION}"
!define SHORT_NAME "Maya NIF Plug-in"

Name "${FULL_NAME}"

Var MAYA_INSTALLDIR

; define installer pages
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_NOAUTOCLOSE

!define MUI_WELCOMEPAGE_TEXT  "This wizard will guide you through the installation of the ${FULL_NAME}.\r\n\r\nBe sure that Maya is closed before you continue.\r\n\r\nNote to Win2k/XP users: Administrative privileges are required to install the ${SHORT_NAME} successfully."
!insertmacro MUI_PAGE_WELCOME

!insertmacro MUI_PAGE_LICENSE ..\license.txt

!define MUI_DIRECTORYPAGE_TEXT_TOP "The field below specifies the folder where the installer has detected your Maya ${MAYA_VERSION} install location. This directory has been detected by analyzing the Windows Registry.$\r$\n$\r$\nFor your convenience, the installer will attempt to remove any old versions of the ${SHORT_NAME} that it finds (no other files will be deleted).$\r$\n$\r$\nUnless you really know what you are doing, you should leave the field below as it is."
!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "Maya ${MAYA_VERSION} Location"
!define MUI_DIRECTORYPAGE_VARIABLE $MAYA_INSTALLDIR
!insertmacro MUI_PAGE_DIRECTORY

!define MUI_DIRECTORYPAGE_TEXT_TOP "Use the field below to specify the folder where you want the documentation files and uninstaller to be copied to. To specify a different folder, type a new name or use the Browse button to select an existing folder."
!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "Documentation & Uninstall Folder"
!define MUI_DIRECTORYPAGE_VARIABLE $INSTDIR
!insertmacro MUI_PAGE_DIRECTORY

!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\Wiki Documentation.URL"
!define MUI_FINISHPAGE_LINK "Visit us at http://niftools.sourceforge.net/"
!define MUI_FINISHPAGE_LINK_LOCATION "http://niftools.sourceforge.net/"
!insertmacro MUI_PAGE_FINISH

!define MUI_WELCOMEPAGE_TEXT  "This wizard will guide you through the uninstallation of the ${FULL_NAME}.\r\n\r\nBefore starting the uninstallation, make sure Maya is not running.\r\n\r\nClick Next to continue."
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

;--------------------------------
; Languages
 
!insertmacro MUI_LANGUAGE "English"
    
;--------------------------------
;Language Strings

;Description
LangString DESC_SecCopyUI ${LANG_ENGLISH} "Copy all required files to the application folder."

;--------------------------------
; Data

OutFile "maya-${MAYA_VERSION}-nif-plug-in-${VERSION}-win.exe"
InstallDir "$PROGRAMFILES\NifTools\${MED_NAME}"
BrandingText "http://niftools.sourceforge.net/"
Icon inst.ico
UninstallIcon inst.ico
ShowInstDetails show
ShowUninstDetails show

;--------------------------------
; Functions

Function .onInit
  ; check if Maya is installed

  ClearErrors
  ReadRegStr $MAYA_INSTALLDIR HKLM "SOFTWARE\Alias|Wavefront\Archive\Maya\${MAYA_VERSION}" "INSTALLDIR"
  IfErrors 0 MayaFound
  
  ClearErrors
  ReadRegStr $MAYA_INSTALLDIR HKLM "SOFTWARE\Alias\Maya\${MAYA_VERSION}\Setup\InstallPath" "MAYA_INSTALL_LOCATION"
  IfErrors 0 MayaFound
  
  ClearErrors
  ReadRegStr $MAYA_INSTALLDIR HKLM "SOFTWARE\Autodesk\Maya\${MAYA_VERSION}\Setup\InstallPath" "MAYA_INSTALL_LOCATION"
  IfErrors 0 MayaFound
  
  ClearErrors
  ReadRegStr $MAYA_INSTALLDIR HKLM "SOFTWARE\Autodesk\Maya\${MAYA_VERSION}\Setup\InstallPath" "MAYA_INSTALL_LOCATION"
  IfErrors 0 MayaFound
  
     ; no key, that means that Maya is not installed
     MessageBox MB_OK "The installer cannot find the Maya ${MAYA_VERSION} install location registry key.  Make sure that Maya ${MAYA_VERSION} is properly installed and try again."
     Abort ; causes installer to quit
     
  MayaFound:

FunctionEnd

Section
  SectionIn RO

  ; Cleanup: remove old versions of the options script from common locations
  SetShellVarContext current
  Delete "$DOCUMENTS\Maya\${MAYA_VERSION}\scripts\nifTranslatorOpts.mel"
  Delete "$DOCUMENTS\Maya\scripts\nifTranslatorOpts.mel"
  
  SetShellVarContext all
  Delete "$MAYA_INSTALLDIR\scripts\others\nifTranslatorOpts.mel"
  
  ;remove old versions of the plug-in MLL file from common locations
  Delete "$MAYA_INSTALLDIR\plugins\nifTranslator.mll"
  Delete "$MAYA_INSTALLDIR\plug-ins\nifTranslator.mll"
 
  ; Install plug-in file
  SetOutPath "$MAYA_INSTALLDIR\plug-ins"
  File "..\..\bin\maya${MAYA_VERSION}\nifTranslator.mll"
  
  ; Install options script
  SetOutPath "$MAYA_INSTALLDIR\scripts\others"
  File ..\nifTranslatorOpts.mel

  ; Install documentation files
  SetOutPath $INSTDIR
  File ..\change_log.txt
  File ..\license.txt
  File "..\Wiki Documentation.URL"

  ; Install shortcuts
  CreateDirectory "$SMPROGRAMS\NifTools\${MED_NAME}\"
  CreateShortCut "$SMPROGRAMS\NifTools\${MED_NAME}\Documentation & Tutorials.lnk" "http://www.niftools.org/wiki/index.php/Maya"
 
  CreateShortCut "$SMPROGRAMS\NifTools\${MED_NAME}\Support.lnk" "http://www.niftools.org/forum/viewforum.php?f=23"
  CreateShortCut "$SMPROGRAMS\NifTools\${MED_NAME}\License.lnk" "$INSTDIR\license .txt"
  CreateShortCut "$SMPROGRAMS\NifTools\${MED_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"

  ; Write the installation path into the registry
  WriteRegStr HKLM "SOFTWARE\NifTools\${MED_NAME}" "INSTALLDIR" "$INSTDIR"
  WriteRegStr HKLM "SOFTWARE\NifTools\${MED_NAME}" "MAYADIR" "$MAYA_INSTALLDIR"

  ; Write the uninstall keys & uninstaller for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MED_NAME}" "DisplayName" "${FULL_NAME} (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MED_NAME}" "UninstallString" "$INSTDIR\uninstall.exe"
  SetOutPath $INSTDIR
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "Uninstall"
  SetShellVarContext all
  SetAutoClose false

  ; recover Blender data dir, where scripts are installed
  ReadRegStr $INSTDIR HKLM "SOFTWARE\NifTools\${MED_NAME}" "INSTALLDIR"
  ReadRegStr $MAYA_INSTALLDIR HKLM "SOFTWARE\NifTools\${MED_NAME}" "MAYADIR"
  
  ; remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MED_NAME}"
  DeleteRegKey HKLM "SOFTWARE\NifTools\${MED_NAME}"

  ; Remove plug-in file
  Delete "$MAYA_INSTALLDIR\bin\plug-ins\nifTranslator.mll"
  
  ; Remove options script
  Delete "$MAYA_INSTALLDIR\scripts\others\nifTranslatorOpts.mel"

  ; remove program files and program directory
  Delete "$INSTDIR\*.*"
  RMDir "$INSTDIR"

  ; remove links in start menu
  Delete "$SMPROGRAMS\NifTools\${MED_NAME}\*.*"
  RMDir "$SMPROGRAMS\NifTools\${MED_NAME}"
  RMDir "$SMPROGRAMS\NifTools" ; this will only delete if the directory is empty
SectionEnd
