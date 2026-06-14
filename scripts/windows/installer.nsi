!define APPNAME "Kayra ROV GCS"
!define COMPANYNAME "Kayra Ekibi"
!define DESCRIPTION "Insansiz Su Alti Araci Kontrol Istasyonu"

InstallDir "$PROGRAMFILES64\${APPNAME}"
Name "${APPNAME}"
OutFile "..\..\Kayra_ROV_Setup.exe"

RequestExecutionLevel admin

Page directory
Page instfiles

Section "Core"
  SetOutPath $INSTDIR
  File "..\..\Kayra_ROV_GCS.exe"
  File "ROV_Baslat.bat"

  ; SDL2 DLL
  File "..\..\windows-sysroot\sdl2-mingw\bin\SDL2.dll"

  ; GStreamer runtime DLLs
  File /nonfatal "..\..\windows-sysroot\gstreamer-rt\gstreamer\1.0\mingw_x86_64\bin\*.dll"

  SetOutPath "$INSTDIR\lib\gstreamer-1.0"
  File /nonfatal "..\..\windows-sysroot\gstreamer-rt\gstreamer\1.0\mingw_x86_64\lib\gstreamer-1.0\*.dll"

  SetOutPath "$INSTDIR\assets"
  File /nonfatal "..\..\assets\*"

  ; Create shortcuts
  CreateShortCut "$DESKTOP\KAYRA ROV Baslat.lnk" "$INSTDIR\ROV_Baslat.bat" "" "$INSTDIR\Kayra_ROV_GCS.exe"
  CreateShortCut "$DESKTOP\KAYRA GCS Arayuz.lnk" "$INSTDIR\Kayra_ROV_GCS.exe" "--gui"

  ; Uninstaller
  SetOutPath $INSTDIR
  WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
  RMDir /r "$INSTDIR"
  Delete "$DESKTOP\KAYRA ROV Baslat.lnk"
  Delete "$DESKTOP\KAYRA GCS Arayuz.lnk"
SectionEnd
